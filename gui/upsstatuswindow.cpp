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

#include "upsstatuswindow.h"
#include "ui_upsstatuswindow.h"
#include "constants.h"
#include <QSettings>
#include <QDateTime>
#include <QMetaEnum>
#include <QScrollBar>
#include <QCloseEvent>
#include <QJsonObject>

UpsStatusWindow::UpsStatusWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::UpsStatusWindow)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    connect(ui->m_closeButton, &QPushButton::clicked, this, &UpsStatusWindow::hide);
    connect(ui->m_saveSettingsButton, &QPushButton::clicked, this, &UpsStatusWindow::saveSettings);

    // loadSettings();
    // Check validation as soon as the selection in the ComboBoxes changes
    connect(ui->m_driverComboBox, &QComboBox::currentIndexChanged, this, &UpsStatusWindow::validateSettings);
    connect(ui->m_comPortComboBox, &QComboBox::currentTextChanged, this, &UpsStatusWindow::validateSettings);

    // Initialize the button status
    validateSettings();
}

UpsStatusWindow::~UpsStatusWindow() {
    delete ui;
}

void UpsStatusWindow::closeEvent(QCloseEvent *event) {
    hide();
    event->ignore();
}

void UpsStatusWindow::loadSettings() {
    // Open the registry where the service writes the settings
    QSettings settings(AppConstants::SETTINGS_SCOPE,
                       AppConstants::APP_ORGANIZATION_NAME,
                       AppConstants::APP_APPLICATION_NAME);

    // Load Shutdown Delay (default to 0 if not found)
    int delay = settings.value(AppConstants::REG_KEY_SHUTDOWN_DELAY, 0).toInt();
    ui->m_shutdownDelaySpinBox->setValue(delay);

    // Load Power Safe Mode (default to false if not found)
    bool isEnabled = settings.value(AppConstants::REG_KEY_POWER_SAFE_ENABLED, false).toBool();
    ui->m_powerSafeCheckBox->setChecked(isEnabled);

    QString savedDriverFile = settings.value(AppConstants::REG_KEY_SELECTED_DRIVER_FILE).toString();
    QString savedPort = settings.value(AppConstants::REG_KEY_SELECTED_COM_PORT).toString();

    // Find the index based on the hidden data (the filename)
    int driverIndex = ui->m_driverComboBox->findData(savedDriverFile);
    if (driverIndex != -1) {
        ui->m_driverComboBox->setCurrentIndex(driverIndex);
    } else {
        qDebug() << "Could not find driver in list:" << savedDriverFile;
    }

    // Find the port based on text (e.g., "COM3")
    int portIndex = ui->m_comPortComboBox->findText(savedPort);
    if (portIndex != -1) {
        ui->m_comPortComboBox->setCurrentIndex(portIndex);
    }
}

void UpsStatusWindow::saveSettings() {
    // 1. Gather the data the user has selected [cite: 11, 12, 13]
    QString selectedDriverFile = ui->m_driverComboBox->currentData().toString();
    QString selectedPort = ui->m_comPortComboBox->currentText();
    int shutdownDelay = ui->m_shutdownDelaySpinBox->value();
    bool powerSafeEnabled = ui->m_powerSafeCheckBox->isChecked();

    // 2. Emit signal only. We do NOT write to QSettings here.
    // The background service will receive this via IPC and handle the Registry.
    emit configurationUpdateRequested(selectedDriverFile, selectedPort, shutdownDelay, powerSafeEnabled);

    // 3. Provide feedback to the user [cite: 3]
    ui->m_statusLabel->setText(tr("Update request sent to service..."));
}

void UpsStatusWindow::updateReport(const UpsReport &report) {
    const UpsData &data = report.data;
    const UpsServiceStatus &service = report.serviceStatus;

    // Update Status
    ui->m_statusLabel->setText(upsStateToString(data.state));

    // Update Voltages
    ui->m_inputVoltageLabel->setText(QString::number(data.inputVoltage, 'f', 1) + " V");
    ui->m_outputVoltageLabel->setText(QString::number(data.outputVoltage, 'f', 1) + " V");
    ui->m_batteryVoltageLabel->setText(QString::number(data.batteryVoltage, 'f', 1) + " V");

    // Service Status
    ui->m_activeDriverNameLabel->setText(service.activeDriverName.isEmpty() ? tr("None") : service.activeDriverName);
    ui->m_activeComPortLabel->setText(service.activeComPort.isEmpty() ? tr("N/A") : service.activeComPort);
    if (!data.statusMessage.isEmpty()) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        QString logEntry = QString("[%1] %2").arg(timestamp, data.statusMessage);

        // Add text without rewriting the entire buffer
        ui->m_rawDataLog->appendPlainText(logEntry);

        // Optional: Limit the history (e.g., 500 lines)
        if (ui->m_rawDataLog->document()->blockCount() > 500) {
            // This prevents the app from becoming slow after running for hours
            // You can also use ui->m_rawDataLog->setMaximumBlockCount(500)
            // call once in the constructor.
        }
    }
}

QString UpsStatusWindow::upsStateToString(UpsMonitor::UpsState state) const {
    QMetaEnum metaEnum = QMetaEnum::fromType<UpsMonitor::UpsState>();
    if (metaEnum.isValid()) {
        const char* key = metaEnum.valueToKey(static_cast<int>(state));
        if (key) return QString("<b>%1</b>").arg(key);
    }
    return tr("Unknown");
}

void UpsStatusWindow::resetLabels() {
    ui->m_statusLabel->setText(tr("Waiting for data..."));
    ui->m_inputVoltageLabel->setText(tr("N/A"));
    ui->m_outputVoltageLabel->setText(tr("N/A"));
    ui->m_batteryVoltageLabel->setText(tr("N/A"));
    ui->m_activeDriverNameLabel->setText(tr("None"));
    ui->m_activeComPortLabel->setText(tr("N/A"));
}

void UpsStatusWindow::setAvailableDrivers(const QHash<QString, QJsonObject> &driverMetadata) {
    ui->m_driverComboBox->clear();
    for (auto it = driverMetadata.constBegin(); it != driverMetadata.constEnd(); ++it) {
        QString fileName = it.key();
        QString friendlyName = it.value().value("displayName").toString();
        if (friendlyName.isEmpty()) {
            friendlyName = fileName;
        }
        // friendlyName is visible, fileName is hidden data
        ui->m_driverComboBox->addItem(friendlyName, fileName);
    }
    loadSettings();
}

void UpsStatusWindow::addComPort(const QString &portName)
{
    // Check if the port is not already in the list to prevent duplicates
    if (ui->m_comPortComboBox->findText(portName) == -1) {
        ui->m_comPortComboBox->addItem(portName);
    }
}

void UpsStatusWindow::validateSettings() {
    // Check if a valid driver is selected (userData must not be empty)
    bool hasDriver = !ui->m_driverComboBox->currentData().toString().isEmpty();

    // Check if port text exists
    bool hasPort = !ui->m_comPortComboBox->currentText().isEmpty();

    // Activate the button only if both are true
    ui->m_saveSettingsButton->setEnabled(hasDriver && hasPort);
}
