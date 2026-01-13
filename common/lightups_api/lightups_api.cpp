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

#include "lightups_api.h"
#include "registry_watcher.h"
#include "constants.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDebug>
#include <QObject>

Ups_api_library::Ups_api_library(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<UpsReport>("UpsReport");

    m_recoveryTimer = new QTimer(this);
    m_recoveryTimer->setSingleShot(true);
    m_recoveryTimer->setInterval(5000);

    // FIX: Use a QueuedConnection for the timer to prevent race conditions
    connect(m_recoveryTimer, &QTimer::timeout, this, &Ups_api_library::loadAndStartDriver, Qt::QueuedConnection);
    connect(this, &Ups_api_library::driverInitSuccess, this, &Ups_api_library::onDriverInitSuccess);
    connect(this, &Ups_api_library::driverInitFailure, this, &Ups_api_library::onDriverInitFailure);
}

Ups_api_library::~Ups_api_library()
{
    if (m_recoveryTimer) m_recoveryTimer->stop();

    if (m_registryThread) {
        m_watcher->stopWatching();
        m_registryThread->quit();
        m_registryThread->wait(2000);
    }
    cleanupDriver();
}

void Ups_api_library::cleanupDriver()
{
    QMutexLocker locker(&m_cleanupMutex);
    qDebug() << "UpsApiLibrary: Starting safe cleanup...";

    if (m_recoveryTimer) m_recoveryTimer->stop();

    // --- CRITICAL ADDITION FOR GUI UPDATE ---
    m_currentStatus.driverLoaded = false;
    m_currentStatus.driverInitialized = false;
    m_currentStatus.dataCommunicationActive = false;
    // Send an empty report: this tells the GUI that the driver is gone
    emitUpsReport(UpsData());
    // --------------------------------------------

    if (m_driver && m_workerThread && m_workerThread->isRunning()) {
        QMetaObject::invokeMethod(m_driver, "stopDriver", Qt::BlockingQueuedConnection);
    }

    if (m_workerThread) {
        m_workerThread->quit();
        if (!m_workerThread->wait(3000)) {
            m_workerThread->terminate();
            m_workerThread->wait();
        }
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    if (m_driver) {
        // Explicitly disconnect old connections before deleting
        disconnect(m_driver, nullptr, this, nullptr);
        delete m_driver;
        m_driver = nullptr;
    }

    if (m_pluginLoader) {
        m_pluginLoader->unload();
        delete m_pluginLoader;
        m_pluginLoader = nullptr;
    }
}

bool Ups_api_library::loadAndStartDriver()
{
    cleanupDriver();

    QSettings settings(AppConstants::SETTINGS_SCOPE, AppConstants::APP_ORGANIZATION_NAME, AppConstants::APP_APPLICATION_NAME);
    settings.sync();

    QString driverFileName = settings.value(AppConstants::REG_KEY_SELECTED_DRIVER_FILE).toString();
    QString comPort = settings.value(AppConstants::REG_KEY_SELECTED_COM_PORT).toString();

    if (driverFileName.isEmpty() || comPort.isEmpty()) {
        onDriverInitFailure(tr("Missing configuration (Driver/Port)"));
        return false;
    }

    m_currentStatus.activeDriverName = driverFileName;
    m_currentStatus.activeComPort = comPort;

    QString pluginPath = QCoreApplication::applicationDirPath() + "/common/plugins/" + driverFileName;

    m_pluginLoader = new QPluginLoader(pluginPath, this);
    QObject *plugin = m_pluginLoader->instance();

    if (!plugin) {
        onDriverInitFailure(tr("Plugin load failed: %1").arg(m_pluginLoader->errorString()));
        return false;
    }

    m_driver = qobject_cast<IUpsDriver*>(plugin);
    if (!m_driver) {
        onDriverInitFailure(tr("Invalid Interface"));
        delete plugin;
        return false;
    }

    m_driver->setParent(nullptr);
    m_driver->moveToThread(this->thread());

    m_currentStatus.driverLoaded = true;
    m_workerThread = new QThread(this);

    // Move to worker thread
    m_driver->moveToThread(m_workerThread);

    // Connections with QueuedConnection for thread safety to the GUI
    connect(m_driver, &IUpsDriver::dataReceived, this, &Ups_api_library::handleDriverData, Qt::QueuedConnection);
    connect(m_driver, &IUpsDriver::initializationFailure, this, &Ups_api_library::driverInitFailure, Qt::QueuedConnection);
    connect(m_driver, &IUpsDriver::initializationSuccess, this, &Ups_api_library::driverInitSuccess, Qt::QueuedConnection);

    connect(m_workerThread, &QThread::started, [this, comPort](){
        if (m_driver) {
            m_driver->initialize(comPort);
        }
    });

    m_workerThread->start();
    return true;
}

void Ups_api_library::onDriverInitFailure(const QString& error)
{
    m_currentStatus.driverInitialized = false;
    m_currentStatus.lastErrorMessage = error;
    emitUpsReport();

    // FIX: Start the timer only if it is not already running to prevent 'spamming'
    if (m_recoveryTimer && !m_recoveryTimer->isActive()) {
        qDebug() << "UpsApiLibrary: Starting recovery timer...";
        m_recoveryTimer->start();
    }
}

void Ups_api_library::onDriverInitSuccess()
{
    m_currentStatus.driverInitialized = true;
    m_currentStatus.lastErrorMessage.clear();
    if (m_recoveryTimer) m_recoveryTimer->stop(); // Stop recovery on success

    // We do not emit a report yet, or we flag it as 'not active'
    m_currentStatus.dataCommunicationActive = false;
    emitUpsReport();
}

void Ups_api_library::handleDriverData(const UpsData& data)
{
    // Once this slot is called, we know the driver has processed a valid D-record.
    m_currentStatus.dataCommunicationActive = true;
    emitUpsReport(data);
}

void Ups_api_library::emitUpsReport(const UpsData& data)
{
    m_currentStatus.timestamp = QDateTime::currentDateTime();
    UpsReport report;
    report.serviceStatus = m_currentStatus;
    report.data = data;

    // If communication is not active (e.g., during init or after cleanup), we overwrite the data with safe 'Unknown' values for the GUI/Tray.
    if (!m_currentStatus.dataCommunicationActive) {
        report.data.statusMessage = tr("No active connection");
        report.data.batteryLevel = 0;
        report.data.state = UpsMonitor::UpsState::Unknown;
        report.data.inputVoltage = 0;
        report.data.outputVoltage = 0;
        report.data.batteryVoltage = 0;
        report.data.temperatureC = 0;
        report.data.loadPercentage = 0;
        report.data.timestamp = QDateTime::currentDateTime();
        report.data.BatteryFault = false;
    }

    emit upsReportAvailable(report);
}

void Ups_api_library::startService()
{
    // Registry watcher setup (simplified for stability)
    if (!m_watcher) {
        m_registryThread = new QThread(this);
        m_watcher = new RegistryWatcher();
        m_watcher->moveToThread(m_registryThread);

        connect(m_watcher, &RegistryWatcher::settingsChanged, this, &Ups_api_library::onRegistryChanged);
        m_registryThread->start();
        QMetaObject::invokeMethod(m_watcher, "startWatching", Qt::QueuedConnection, Q_ARG(QString, ""));
    }

    loadAndStartDriver();
}

void Ups_api_library::onRegistryChanged()
{
    QSettings settings(AppConstants::SETTINGS_SCOPE,
                       AppConstants::APP_ORGANIZATION_NAME,
                       AppConstants::APP_APPLICATION_NAME);
    settings.sync();

    QString newDriver = settings.value(AppConstants::REG_KEY_SELECTED_DRIVER_FILE).toString();
    QString newPort = settings.value(AppConstants::REG_KEY_SELECTED_COM_PORT).toString();

    // CHECK: Is the driver or the port actually different from what is currently running?
    if (newDriver != m_currentStatus.activeDriverName || newPort != m_currentStatus.activeComPort) {
        qDebug() << "UpsApiLibrary: Hardware settings changed. Restarting driver...";
        loadAndStartDriver();
    } else {
        qDebug() << "UpsApiLibrary: Only logical settings changed. No driver restart required.";
    }
}
