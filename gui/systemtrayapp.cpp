/*
 * LightUps: A lightweight Qt-based UPS monitoring service and client for Windows.
 * Copyright (C) 2026 Andreas Hoogendoorn (@andhoo)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "systemtrayapp.h"
#include <QLocalSocket>
#include <QStyle>
#include <QDebug>
#include <QWidget>
#include <QGuiApplication>
#include <QScreen>
#include <QPluginLoader>
#include <QDir>
#include <QMessageBox>
#include <QSerialPortInfo>
#include "constants.h"
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QDataStream>
#include "constants.h"
#include "ipc_constants.h"
#include "ups_report.h"

SystemTrayApp::SystemTrayApp(QApplication *app, QObject *parent)
    // ==========================================================
    // 1. Initializer List (All members must be created HERE)
    // ==========================================================
    : QObject(parent),
    m_app(app),
    // The iconManager is now created ONCE and early
    m_iconManager(new UpsIconManager(this)),
    m_settings(new QSettings(AppConstants::SETTINGS_SCOPE,
                             AppConstants::APP_ORGANIZATION_NAME,
                             AppConstants::APP_APPLICATION_NAME,
                             this)),
    // m_systemActions(new SystemActions(this)),

    // IPC objects
    m_localSocket(new QLocalSocket(this)),
    m_reconnectTimer(new QTimer(this))
// ==========================================================
// 2. Constructor Body
// ==========================================================
{
    // --- Meta Type Registration ---
    // Required for custom types in signal/slot throughout the system
    qRegisterMetaType<UpsMonitor::UpsState>("UpsStatus");
    m_lastReport = UpsReport(); // Initialize the last report
    createTrayIcon();
    createTrayMenu();
    loadAvailableDriversMetadata();
    m_statusWindow = new UpsStatusWindow();
    m_statusWindow->hide();

    // --- IPC Client Configuration ---
    m_reconnectTimer->setInterval(5000); // Retry every 5 seconds

    // --- IPC Connections ---
    connect(m_localSocket, &QLocalSocket::connected, this, &SystemTrayApp::socketConnected);
    connect(m_localSocket, &QLocalSocket::disconnected, this, &SystemTrayApp::socketDisconnected);
    // Use QOverload for the overloaded errorOccurred method
    connect(m_localSocket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::errorOccurred),
            this, &SystemTrayApp::socketError);
    connect(m_localSocket, &QLocalSocket::readyRead, this, &SystemTrayApp::socketReadyRead);
    connect(m_reconnectTimer, &QTimer::timeout, this, &SystemTrayApp::connectToServer);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayApp::trayIconActivated);

    // Start the monitoring service (starts the IPC connection)
    connectToServer();

    // In the SystemTrayApp constructor, connect the new signal:
    connect(m_statusWindow, &UpsStatusWindow::configurationUpdateRequested,
            this, [this](const QString &driver, const QString &port, int delay, bool powerSafe) {
                sendFullConfiguration(driver, port, delay, powerSafe);
            });
    connect(m_statusWindow, &UpsStatusWindow::configurationChanged, this, [this](const QString &driver, const QString &port, int delay, bool powerSafe){
        // 1. Send directly to the service via IPC
        sendFullConfiguration(driver, port, delay, powerSafe);

        // 2. Optionally show a notification to the user
        // showNotification(tr("Settings updated"),
        //                                        tr("The new configuration has been sent to the UPS service."));
    });
}

SystemTrayApp::~SystemTrayApp()
{
    delete m_trayMenu;
    delete m_trayIcon;
    delete m_statusWindow;
}

// ----------------------------------------------------
// PRIVATE SLOTS (IPC COMMUNICATION)
// ----------------------------------------------------

void SystemTrayApp::connectToServer()
{
    QLocalSocket::LocalSocketState currentState = m_localSocket->state();

    // Only try to connect if we are NOT already connected or connecting.
    if (currentState != QLocalSocket::ConnectedState &&
        currentState != QLocalSocket::ConnectingState) {

        qDebug() << "SystemTrayApp: Connecting to IPC server:" << IPC_SERVER_NAME
                 << " (Current state:" << currentState << ")";

        // Stop the timer, as we are now starting a connection attempt.
        m_reconnectTimer->stop();

        // If the socket is in ClosingState (or another unwanted remnant),
        // we call abort() to reset it cleanly to UnconnectedState.
        if (currentState == QLocalSocket::ClosingState) {
            m_localSocket->abort();
        }

        // Start the asynchronous connection attempt.
        m_localSocket->connectToServer(IPC_SERVER_NAME);
    }
}

void SystemTrayApp::socketConnected()
{
    qDebug() << "SystemTrayApp: Connection to IPC server SUCCESSFUL.";
    m_reconnectTimer->stop();
    m_nextBlockSize = 0; // Reset the buffer size
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(
            tr("Connection Restored"),
            tr("The connection to the background service has been successfully restored."),
            QSystemTrayIcon::Information,
            3000 // Show the notification for 3 seconds
            );
    } else {
        qDebug() << "Cannot show notification: SystemTrayIcon not found or not visible.";
    }
    updateTrayIconStatus();
}

void SystemTrayApp::socketDisconnected()
{
    qDebug() << "SystemTrayApp: Connection to IPC server lost. Retrying in 5 seconds...";
    m_nextBlockSize = 0;

    // 1. Reset the Service Status (Communication error)
    m_lastReport.serviceStatus.dataCommunicationActive = false;
    m_lastReport.serviceStatus.driverInitialized = false;
    m_lastReport.serviceStatus.lastErrorMessage = tr("IPC Connection lost.");

    // 2. Reset the UPS Hardware Data (to remove numerical 'stale' values)
    // This sets V, I, Temp to 0 and the state to UpsState::Unknown (thanks to the default in UpsData).
    m_lastReport.data = UpsData();

    updateTrayIconStatus(); // Updates the icon to 'Unknown' (due to the fix in determineRequiredIconStatus)

    // 3. >>> IMPORTANT FIX: Force the Diagnostics Window to show the reset data.
    if (m_statusWindow) {
        // Calls the method also used in handleUpsReport.
        m_statusWindow->updateReport(m_lastReport);
    }

    // 4. Start the reconnection attempts
    m_reconnectTimer->start();
}

void SystemTrayApp::socketReadyRead()
{
    QDataStream in(m_localSocket);
    in.setVersion(QDataStream::Qt_6_0);

    for (;;) {
        // 1. Read the block size (if not already done)
        if (m_nextBlockSize == 0) {
            if (m_localSocket->bytesAvailable() < (qint64)sizeof(quint32))
                break;

            in >> m_nextBlockSize;
        }

        // 2. Wait until the entire block is received
        if (m_localSocket->bytesAvailable() < m_nextBlockSize)
            break;

        // 3. Read the data
        UpsReport report;
        // The operator>>(QDataStream&, UpsReport&) reads the data into the struct
        in >> report;

        // Check if reading succeeded
        if (in.status() == QDataStream::Ok) {
            handleUpsReport(report); // Process the received report
            m_nextBlockSize = 0; // Reset for the next block
        } else {
            qDebug() << "SystemTrayApp: QDataStream error while reading the report.";
            m_nextBlockSize = 0;
            break;
        }
    }
}

void SystemTrayApp::socketError(QLocalSocket::LocalSocketError socketError)
{
    // Ignore an error if we are still trying to connect or already connected.
    if (m_localSocket->state() != QLocalSocket::ConnectingState &&
        m_localSocket->state() != QLocalSocket::ConnectedState)
    {
        QString errorMsg;
        switch (socketError) {
        case QLocalSocket::ServerNotFoundError:
            errorMsg = tr("Service not found. Is the background service running?");
            break;
        case QLocalSocket::ConnectionRefusedError:
            errorMsg = tr("Connection refused. Is the service busy?");
            break;
        case QLocalSocket::PeerClosedError:
            errorMsg = tr("SystemTrayApp: Peer closed.");
            return;
        case QLocalSocket::UnknownSocketError:
        default:
            errorMsg = m_localSocket->errorString();
            break;
        }

        qDebug() << "SystemTrayApp: IPC ERROR:" << errorMsg;

        m_lastReport.serviceStatus.driverInitialized = false;
        m_lastReport.serviceStatus.lastErrorMessage = tr("IPC ERROR: ") + errorMsg;
        updateTrayIconStatus();
        if (m_localSocket->state() != QLocalSocket::ConnectedState) {
            m_reconnectTimer->start();
        }
    }
}


// ----------------------------------------------------
// PRIVATE SLOTS (TRAY ICON & MENU)
// ----------------------------------------------------

void SystemTrayApp::openSmallWindow() {

    // 1. Fill available drivers
    m_statusWindow->setAvailableDrivers(m_driverMetadata);

    // 2. Fill COM ports via the public method (NOT via ui->)
    // First clear old items if necessary (optional, depending on your addComPort logic)
    // We retrieve the ports via QSerialPortInfo
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        m_statusWindow->addComPort(port.portName());
    }

    // 3. Load the settings from the registry to select the correct items
    m_statusWindow->loadSettings();

    m_statusWindow->show();
}

void SystemTrayApp::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        // Single click shows/hides the diagnostics window
        if (m_statusWindow->isVisible()) {
            m_statusWindow->hide();
        } else {
            openSmallWindow();
        }
    }
}

/**
 * @brief Receives the combined reporting from the IPC client.
 */
void SystemTrayApp::handleUpsReport(const UpsReport &report)
{
    // Check if we are switching from AC power to battery
    if (m_lastReport.data.state != UpsMonitor::UpsState::OnBattery &&
        report.data.state == UpsMonitor::UpsState::OnBattery) {

        m_trayIcon->showMessage(
            tr("Power Outage!"),
            tr("The UPS is now running on battery. Save your work."),
            QSystemTrayIcon::Warning,
            10000 // Show for 10 seconds
            );
    }

    // 1. Save the new status and data
    m_lastReport = report;

    // 2. Forward the data to the Diagnostics Window
    if (m_statusWindow) {
        m_statusWindow->updateReport(report);
    }

    // 3. Update the Tray Icon
    updateTrayIconStatus();
}

void SystemTrayApp::createTrayIcon()
{
    // 1. Create the object
    m_trayIcon = new QSystemTrayIcon(this);

    // 2. Create the icon (uses the manager)
    // The initial status is 'Unknown' (empty, gray battery)
    QIcon initialIcon = m_iconManager->getIconForStatus(UpsMonitor::UpsState::Unknown);

    // 3. ASSIGN the icon
    if (!initialIcon.isNull()) { // <-- ADD THIS CHECK
        m_trayIcon->setIcon(initialIcon);
    } else {
        qDebug() << "Error: Icon from UpsIconManager is invalid. Check SVG path.";
        // You can set a fallback default icon here (such as a Qt standard icon)
        m_trayIcon->setIcon(m_app->style()->standardIcon(QStyle::SP_ComputerIcon));
    }

    // 4. Call show() AFTER the icon is set
    m_trayIcon->show(); // <<< Ensure this is called
    updateTrayIconTooltip();
}

void SystemTrayApp::createTrayMenu()
{
    // if (m_trayMenu) delete m_trayMenu;
    m_trayMenu = new QMenu();
    QStyle *style = m_app->style();

    // --- 1. Diagnostics / Main Window ---
    QIcon diagIcon = style->standardIcon(QStyle::SP_TitleBarMenuButton);
    QAction *openWindowAction = m_trayMenu->addAction(diagIcon, tr("Diagnostics and Status..."));
    connect(openWindowAction, &QAction::triggered, this, &SystemTrayApp::openSmallWindow);
    m_trayMenu->addSeparator();

    // --- 2. About ---
    QAction *aboutAction = m_trayMenu->addAction(tr("About LightUps..."));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        if (m_aboutBox) {
            m_aboutBox->activateWindow();
            m_aboutBox->raise();
            return;
        }
        m_aboutBox = new QMessageBox(nullptr);
        m_aboutBox->setAttribute(Qt::WA_DeleteOnClose);
        m_aboutBox->setWindowTitle(tr("About LightUps"));
        m_aboutBox->setWindowModality(Qt::NonModal);
        m_aboutBox->setStandardButtons(QMessageBox::Ok);
        m_aboutBox->setWindowFlags(m_aboutBox->windowFlags() | Qt::WindowStaysOnTopHint);
        // m_aboutBox->setIconPixmap(QPixmap(":/assets/ups_icon.svg").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        QString aboutText = tr("<div style='text-align: center;'>"
                               "<h2>LightUps</h2>"
                               "<p><b>Version %1</b></p>"
                               "<p>A lightweight UPS monitoring client for Windows.</p>"
                               "<p>Copyright &copy; %2 Andreas Hoogendoorn (@andhoo)</p>"
                               "<p><a href='https://github.com/light-ups/lightups'>Visit Project Website</a></p>"
                               "</div>"
                               "<hr>"
                               "<p style='font-size: small;'>This program is free software: you can redistribute it and/or modify "
                               "it under the terms of the <b>GNU Affero General Public License (AGPLv3)</b>.</p>"
                               "<p style='font-size: x-small; color: #666;'>"
                               "This application uses the <b>Qt Toolkit</b> under the terms of the GNU LGPLv3.</p>")
                                .arg(APP_VERSION)
                                .arg(COPYRIGHT_YEAR);
        m_aboutBox->setText(aboutText);
        m_aboutBox->setTextFormat(Qt::RichText);
        m_aboutBox->show();
    });
    m_trayMenu->addSeparator();

    // --- 3. Exit ---
    QIcon quitIcon = style->standardIcon(QStyle::SP_TitleBarCloseButton);
    QAction *quitAction = m_trayMenu->addAction(quitIcon, tr("Exit"));
    connect(quitAction, &QAction::triggered, m_app, &QApplication::quit);

    // if (m_trayIcon) {
        m_trayIcon->setContextMenu(m_trayMenu);
    // }
}

void SystemTrayApp::loadAvailableDriversMetadata()
{
    m_driverMetadata.clear();
    // Search in the plugins folder
    QDir pluginsDir(QCoreApplication::applicationDirPath() + "/common/plugins");
    for (const QString &fileName : pluginsDir.entryList(QDir::Files)) {
        QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
        QJsonObject metaData = loader.metaData();
        if (!metaData.isEmpty() && metaData.contains("MetaData")) {
            QJsonObject pluginContent = metaData["MetaData"].toObject();
            if (pluginContent.contains("displayName")) {
                // Save the filename so we know which .dll to load
                pluginContent["driverFileName"] = fileName;
                m_driverMetadata.insert(fileName, pluginContent);
            }
        }
    }
}

/**
 * @brief Send a configuration update to the IPC service.
 * @param key The QSettings key (e.g., "REG_KEY_SELECTED_COM_PORT").
 * @param value The value (e.g., "COM3").
 */
void SystemTrayApp::notifyService(const QString &key, const QString &value)
{
    // 1. Safety check on the socket
    if (!m_localSocket || m_localSocket->state() != QLocalSocket::ConnectedState) {
        qDebug() << "SystemTrayApp: Cannot send config: IPC not connected.";
        return;
    }

    // 2. Create the payload (the actual data)
    QByteArray payload;
    QDataStream payloadStream(&payload, QIODevice::WriteOnly);
    payloadStream.setVersion(QDataStream::Qt_6_0); // Or 5_15, as long as the service version is the same
    QMap<QString, QString> command;
    command.insert("COMMAND", "CONFIG_UPDATE");
    command.insert(key, value);
    payloadStream << command;

    // 3. Create the final packet (Size + Payload)
    QByteArray finalBlock;
    QDataStream out(&finalBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);

    // payload size, then the payload itself
    out << (quint32)payload.size();
    finalBlock.append(payload);

    // 4. Send
    m_localSocket->write(finalBlock);
    m_localSocket->flush();
    qDebug() << "SystemTrayApp: Config-update sent via IPC:" << key << "=" << value;
}

/**
 * @brief Determines the required visual status of the icon based on the latest report.
 */

UpsMonitor::UpsState SystemTrayApp::determineRequiredIconStatus() const
{
    // ====================================================================
    // STEP 1: HIGHEST PRIORITY: COMMUNICATION STATUS (Service Down/Not Connected)
    // ====================================================================

    // The 'dataCommunicationActive' flag is set to FALSE in socketDisconnected().
    // This catches:
    // 1. The initial state of the app (if correctly initialized).
    // 2. The state after a QLocalSocket disconnect.
    if (!m_lastReport.serviceStatus.dataCommunicationActive) {

        // Even if m_lastReport.data.state still contains 'OnlineFull' (stale data),
        // we ignore this and force the 'Unknown' status.
        return UpsMonitor::UpsState::Unknown;
    }

    // ====================================================================
    // STEP 2: LOW PRIORITY: UPS-HARDWARE STATUS (Communication OK)
    // ====================================================================

    // If communication is active, we use the last received UPS status.
    UpsState actualState = m_lastReport.data.state;

    // The direct conversion/cast is safe because the enums are identical in name and order.
    return static_cast<UpsMonitor::UpsState>(actualState);
}


/**
 * @brief The central function to update the system tray icon.
 */

void SystemTrayApp::updateTrayIconStatus()
{
    // Step 1: Determine the required status
    UpsMonitor::UpsState requiredStatus = determineRequiredIconStatus();

    // Step 2: Request the QIcon from the Manager.
    // We use 16x16 as a base; the manager scales this for HiDPI.
    QIcon newIcon = m_iconManager->getIconForStatus(requiredStatus, QSize(16, 16));

    // Step 3: Update the System Tray icon
    if (!newIcon.isNull()) {
        m_trayIcon->setIcon(newIcon);
    } else {
        // Fall back on a standard or error icon
        m_trayIcon->setIcon(m_app->style()->standardIcon(QStyle::SP_DriveFDIcon));
    }
    updateTrayIconTooltip(); // The tooltip is updated separately
}

/**
 * @brief Update the text that appears when hovering over the icon.
 */

void SystemTrayApp::updateTrayIconTooltip()
{
    // 0. CHECK A: IPC CONNECTION ERROR (HIGHEST PRIORITY)
    // If the socket does not exist, or is not in connected state, AND we are NOT connecting.
    if (!m_localSocket ||
        (m_localSocket->state() != QLocalSocket::ConnectedState &&
         m_localSocket->state() != QLocalSocket::ConnectingState))
    {
        m_trayIcon->setToolTip(
            tr("🛑 Error: No connection to the background service.") +
            tr("\n\nThe monitor service might not be running or is unreachable.")            );
        return;
    }

    // 0. CHECK B: WAITING FOR INITIAL DATA
    // If we are connected or connecting, but the timestamp is invalid (default status).
    if (m_localSocket->state() == QLocalSocket::ConnectingState ||
        !m_lastReport.data.timestamp.isValid())
    {
        m_trayIcon->setToolTip(tr("UPS Monitor: Connecting or waiting for initial data..."));
        return;
    }

    // Use the last received data from m_lastReport
    const UpsData& data = m_lastReport.data;
    const UpsServiceStatus& service = m_lastReport.serviceStatus;

    QString tooltip;

    // --- SCENARIO 3: Error Handling (Highest Priority) ---

    // 1. Service/Driver Error
    if (!service.driverInitialized) {
        // Error in the service/driver initialization or communication
        tooltip = tr("⚠️ Error: %1\n(Plugin: %2\nPort: %3)")
                      .arg(service.lastErrorMessage.isEmpty() ? tr("Communication/Driver not initialized") : service.lastErrorMessage,
                           service.activeDriverName.isEmpty() ? tr("Unknown") : service.activeDriverName,
                           service.activeComPort.isEmpty() ? tr("N/A") : service.activeComPort);
    }
    // 2. Critical UPS Error (BatteryLow)
    else if (data.state == UpsState::BatteryCritical) {
        // This is the critical status based on the flag (Bit 1)
        tooltip = tr("🔴 CRITICAL ERROR: Battery Low\nShutdown Required!\nBattery Voltage: %1 V")
                      .arg(data.batteryVoltage, 0, 'f', 1);
    }
    // 3. UPS Running on Battery (OnBattery)
    else if (data.state == UpsState::OnBattery) {
        // Notification, voltage and charge
        tooltip = QString(tr("🔋 Power Loss Detected!\nBattery Voltage: %1 V\nRemaining Charge: %2 %%"))
                      .arg(data.batteryVoltage, 0, 'f', 1)
                      .arg(data.batteryLevel, 0, 'f', 1);

    }
    // 4. Online Warning (Frequency Not In Sync)
    else if (data.state == UpsState::OnlineFault) {
        // Specifically for Frequency Async issues
        tooltip = tr("⚠️ Warning: UPS Frequency not in sync with mains!\nInput: %1 V\nBattery: %2 V")
                      .arg(data.inputVoltage, 0, 'f', 1)
                      .arg(data.batteryVoltage, 0, 'f', 1);
    }
    // 5. Battery is Charging
    else if (data.state == UpsState::OnlineCharging) {
        tooltip = QString(tr("✅ Battery Charging\nInput: %1 V\nBattery: %2 V"))
                      .arg(data.inputVoltage, 0, 'f', 1)
                      .arg(data.batteryVoltage, 0, 'f', 1);

    }
    // 6. Online Normal
    else if (data.state == UpsState::OnlineFull) {
        tooltip = QString(tr("✅ On Main Power (Online)\nInput: %1 V\nBattery: %2 V"))
                      .arg(data.inputVoltage, 0, 'f', 1)
                      .arg(data.batteryVoltage, 0, 'f', 1);
    }
    // 7. Other Statuses (Fallback)
    else {
        // Catches any other unknown statuses (such as an unknown Fault code)
        tooltip = QString(tr("☝ Status: %1\n(Voltage: %2 V)"))
                      .arg(data.statusMessage)
                      .arg(data.batteryVoltage, 0, 'f', 1);
    }
    m_trayIcon->setToolTip(tooltip);
}

void SystemTrayApp::sendFullConfiguration(const QString &driver, const QString &port, int delay, bool powerSafe)
{
    if (!m_localSocket || m_localSocket->state() != QLocalSocket::ConnectedState) {
        qDebug() << "SystemTrayApp: IPC not connected.";
        return;
    }

    // 1. Create the payload with ALL data
    QByteArray payload;
    QDataStream payloadStream(&payload, QIODevice::WriteOnly);
    payloadStream.setVersion(QDataStream::Qt_6_0);
    QMap<QString, QString> command;
    command.insert("COMMAND", "CONFIG_UPDATE");
    // We now add both settings to the same map
    command.insert(AppConstants::REG_KEY_SELECTED_DRIVER_FILE, driver);
    command.insert(AppConstants::REG_KEY_SELECTED_COM_PORT, port);
    // NEW: Add the new settings to the IPC packet
    command.insert(AppConstants::REG_KEY_SHUTDOWN_DELAY, QString::number(delay));
    command.insert(AppConstants::REG_KEY_POWER_SAFE_ENABLED, powerSafe ? "true" : "false");
    payloadStream << command;

    // 2. Construct packet (Header with size + Payload)
    QByteArray finalBlock;
    QDataStream out(&finalBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << (quint32)payload.size();
    finalBlock.append(payload);

    // 3. Send
    m_localSocket->write(finalBlock);
    m_localSocket->flush();
    qDebug() << "SystemTrayApp: Full configuration sent:" << driver << "on" << port;
}
