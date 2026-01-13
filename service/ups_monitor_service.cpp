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

#include "ups_monitor_service.h"
#include "windows_service.h"
#include <QDebug>
#include <QProcess>
#include <QSettings>
#include "constants.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

UpsMonitorCore::UpsMonitorCore(QObject *parent)
    : QObject(parent),
    m_shutdownTimer(new QTimer(this)),
    m_cpuRecoveryTimer(new QTimer(this)),
    m_isTimerRunning(false),
    m_lastState(UpsMonitor::UpsState::Unknown),
    m_currentPowerModeIsBattery(false)
{
    // 1. Initialize the registry (creates the keys and keywords)
    initializeRegistry();

    // 2. Load the values we just created (or that were already there)
    loadSettings();

    // 3. Other timer setup
    m_shutdownTimer->setSingleShot(true);
    m_shutdownTimer->setInterval(30000);
    connect(m_shutdownTimer, &QTimer::timeout, this, &UpsMonitorCore::executeShutdown);

    m_cpuRecoveryTimer->setSingleShot(true);
    m_cpuRecoveryTimer->setInterval(10000);
    connect(m_cpuRecoveryTimer, &QTimer::timeout, this, &UpsMonitorCore::restoreCpuSpeed);

    // --- PROPOSAL: Direct check upon startup ----
    checkAndFixPowerProfile();
}

void UpsMonitorCore::checkAndFixPowerProfile()
{
#ifdef Q_OS_WIN
    QProcess process;
    process.start("powercfg", {"/getactivescheme"});
    process.waitForFinished();
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).toLower();

    // GUID for Power Saver
    QString powerSaverGuid = "a1841308-3541-4fab-bc81-f71556f20b4a";

    if (output.contains(powerSaverGuid)) {
        qDebug() << "Service Start: System detected in Power Saver mode. Restoring...";
        setPowerMode(false); // Force to Balanced
    } else {
        qDebug() << "Service Start: Power profile is already correct.";
        m_currentPowerModeIsBattery = false;
    }
#endif
}

void UpsMonitorCore::handleUpsReport(const UpsReport &report)
{
    using namespace UpsMonitor;

    // --- THE GATEKEEPER ---
    // When communication is lost (e.g., when changing drivers):
    if (!report.serviceStatus.dataCommunicationActive) {
        // If a shutdown timer was running, stop it for safety
        if (m_isTimerRunning) {
            m_shutdownTimer->stop();
            m_isTimerRunning = false;
            qDebug() << "Service: Communication lost, shutdown timer stopped.";
        }
        m_lastState = UpsState::Unknown;
        return; // Do not process the rest of the logic
    }

    UpsState currentState = report.data.state;

    // 1. Ignore Unknown (already handled by the gatekeeper above)
    if (currentState == UpsState::Unknown) return;

    // 2. Only take action on an actual state change
    if (currentState == m_lastState) {
        return;
    }

    // Update the last known state
    m_lastState = currentState;

    // --- EVENT VIEWER LOGGING LOGIC ---
    QString logMsg = tr("UPS Status changed to: ");
    WORD logType = EVENTLOG_INFORMATION_TYPE;
    DWORD eventId = UpsEvents::ID_SERVICE_INFO; // Default ID
    // Determine the text and severity of the event based on the new status
    switch(currentState) {
    case UpsState::OnBattery:
        logMsg += tr("On Battery (Power failure!)");
        logType = EVENTLOG_WARNING_TYPE;
        eventId = UpsEvents::ID_ON_BATTERY;
        break;
    case UpsState::BatteryCritical:
        logMsg += tr("Battery Critical (System shutdown imminent!)");
        logType = EVENTLOG_ERROR_TYPE;
        eventId = UpsEvents::ID_BATT_CRITICAL;
        break;
    case UpsState::OnlineFull:
        logMsg += tr("Online (Battery fully charged)");
        eventId = UpsEvents::ID_POWER_RESTORED;
        break;
    case UpsState::OnlineCharging:
        logMsg += tr("Online (Battery charging)");
        eventId = UpsEvents::ID_POWER_RESTORED;
        break;
    case UpsState::OnlineFault:
        logMsg += tr("Online with fault (Check hardware)");
        logType = EVENTLOG_WARNING_TYPE;
        eventId = UpsEvents::ID_SERVICE_ERROR;
        break;
    default:
        logMsg += tr("Unknown status detected. Check UPS connection.");
        eventId = UpsEvents::ID_SERVICE_ERROR;
        break;
    }

    // Send the log to the Windows Event Viewer via our helper function
#ifdef Q_OS_WIN
    WindowsService::logEvent(logMsg, logType, eventId);
#else
    qDebug() << logMsg;
#endif
    // Situation: Power failure or a critical error
    if (currentState == UpsState::OnBattery || currentState == UpsState::BatteryCritical || currentState == UpsState::OnlineFault) {
        // Stop the recovery timer if it was running (as we just switched to battery)
        if (m_cpuRecoveryTimer->isActive()) m_cpuRecoveryTimer->stop();

        // Set the system to power saver mode immediately
        setPowerMode(true);

        if (!m_isTimerRunning) {
            // Only start the shutdown timer if a delay is configured in settings
            if (m_shutdownDelay > 0) {
                m_shutdownTimer->start();
                m_isTimerRunning = true;
                qDebug() << "Shutdown timer started for" << m_shutdownDelay << "seconds.";
            } else {
                qDebug() << "UPS on battery, but shutdown is disabled (delay = 0).";
            }
        }
    }
    // Situation: Power restored
    else if (currentState == UpsState::OnlineFull || currentState == UpsState::OnlineCharging) {
        // Cancel the shutdown if it was scheduled
        if (m_isTimerRunning) {
            m_shutdownTimer->stop();
            m_isTimerRunning = false;
            WindowsService::logEvent(QCoreApplication::translate("UpsMonitorCore","Power restored: Scheduled shutdown cancelled."));
        }

        // Start a short timer before the CPU returns to full speed (prevents spikes during flickering)
        if (!m_cpuRecoveryTimer->isActive()) {
            m_cpuRecoveryTimer->start();
        }
    }
}

void UpsMonitorCore::setPowerMode(bool batteryMode)
{
    // Prevent duplicate calls if the state is already correct
    if (m_lastState != UpsMonitor::UpsState::Unknown && m_currentPowerModeIsBattery == batteryMode) {
        return;
    }

#ifdef Q_OS_WIN
    QString guid = batteryMode ? "a1841308-3541-4fab-bc81-f71556f20b4a"
                               : "381b4222-f694-41f0-9685-ff5bb260df2e";

    QProcess::startDetached("powercfg", {"/setactive", guid});
    m_currentPowerModeIsBattery = batteryMode;
    qDebug() << "System: Power Scheme changed to" << (batteryMode ? "Power Saver" : "Balanced");
#endif
}

void UpsMonitorCore::restoreCpuSpeed()
{
    setPowerMode(false);
}

void UpsMonitorCore::executeShutdown()
{
    qDebug() << "CRITICAL: Shutdown initiated.";
    setPowerMode(false); // Always revert to balanced for the next boot

#ifdef Q_OS_WIN
    QProcess::startDetached("shutdown", {"/s", "/f", "/t", "0"});
#endif
}

void UpsMonitorCore::loadSettings() {
    QSettings settings(AppConstants::SETTINGS_SCOPE,
                       AppConstants::APP_ORGANIZATION_NAME,
                       AppConstants::APP_APPLICATION_NAME);
    int oldDelay = m_shutdownDelay;
    bool oldPowerSafe = m_powerSafeEnabled;
    m_shutdownDelay = settings.value(AppConstants::REG_KEY_SHUTDOWN_DELAY, 30).toInt();
    m_powerSafeEnabled = settings.value(AppConstants::REG_KEY_POWER_SAFE_ENABLED, false).toBool();

    // 1. UPDATE DELAY ON-THE-FLY
    m_shutdownTimer->setInterval(m_shutdownDelay * 1000);
    if (m_isTimerRunning) {
        // If the new delay is 0, stop the timer immediately (shutdown cancelled)
        if (m_shutdownDelay <= 0) {
            m_shutdownTimer->stop();
            m_isTimerRunning = false;
            qDebug() << "ON-THE-FLY: Shutdown cancelled (delay set to 0).";
        }
        // Otherwise, if the delay has changed, restart with the new time
        else if (oldDelay != m_shutdownDelay) {
            m_shutdownTimer->stop();
            m_shutdownTimer->start();
            qDebug() << "ON-THE-FLY: Timer restarted with new interval:" << m_shutdownDelay << "s";
        }
    }

    // 2. UPDATE POWERSAFE ON-THE-FLY
    if (oldPowerSafe && !m_powerSafeEnabled && m_currentPowerModeIsBattery) {
        qDebug() << "ON-THE-FLY: PowerSafe disabled. Restoring balanced power profile immediately.";
        setPowerMode(false);
    }
    else if (!oldPowerSafe && m_powerSafeEnabled && m_lastState == UpsMonitor::UpsState::OnBattery) {
        qDebug() << "ON-THE-FLY: PowerSafe enabled. Activating power saver mode.";
        setPowerMode(true);
    }

    // Clear log upon start/update
    qDebug() << "-----------------------------------------------";
    qDebug() << "UPS Monitor Service Configuration loaded:";
    qDebug() << " - Shutdown Delay: " << m_shutdownDelay << (m_shutdownDelay <= 0 ? " (DISABLED)" : " s");
    qDebug() << " - PowerSafe Mode: " << (m_powerSafeEnabled ? "ON" : "OFF");
    qDebug() << "-----------------------------------------------";
}

void UpsMonitorCore::initializeRegistry() {
    // Use explicit HKLM path to avoid 32/64-bit confusion
    QString regPath = QString("HKEY_LOCAL_MACHINE\\Software\\%1\\%2")
                          .arg(AppConstants::APP_ORGANIZATION_NAME, AppConstants::APP_APPLICATION_NAME);
    QSettings settings(regPath, QSettings::NativeFormat);

    // Check if a base key exists. If not, create everything.
    if (!settings.contains(AppConstants::REG_KEY_SHUTDOWN_DELAY)) {
        qDebug() << "Registry: Creating default settings in HKLM...";
        settings.setValue(AppConstants::REG_KEY_SHUTDOWN_DELAY, 30);
        settings.setValue(AppConstants::REG_KEY_POWER_SAFE_ENABLED, false);
        settings.setValue(AppConstants::REG_KEY_SELECTED_DRIVER_FILE, "template_driver.dll");
        settings.setValue(AppConstants::REG_KEY_SELECTED_COM_PORT, "COM1");
        settings.sync(); // Force write to Windows
        qDebug() << "Registry: Default values successfully written.";
    } else {
        qDebug() << "Registry: Existing settings found in HKLM.";
    }
}
