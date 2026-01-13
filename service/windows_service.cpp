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

#include "windows_service.h"
#include "lightups_api.h"
#include "ups_ipc_server.h"
#include "ups_monitor_service.h"
#include "constants.h"
#include <QDebug>
#include <QDir>

// Use the global context defined in main.cpp
extern AppContext g_context;

SERVICE_STATUS        WindowsService::m_status = {0};
SERVICE_STATUS_HANDLE WindowsService::m_statusHandle = nullptr;

QCoreApplication* WindowsService::m_app = nullptr;
int WindowsService::m_argc = 0;
char** WindowsService::m_argv = nullptr;

bool WindowsService::run(int argc, char *argv[]) {
    m_argc = argc;
    m_argv = argv;

    // registerEventSource();
    // Use a consistent name from constants
    std::wstring serviceName = AppConstants::APP_APPLICATION_NAME.toStdWString();

    SERVICE_TABLE_ENTRY dispatchTable[] = {
        { (LPWSTR)serviceName.c_str(), (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };
    return StartServiceCtrlDispatcher(dispatchTable);
}

void WINAPI WindowsService::ServiceMain(DWORD argc, LPTSTR *argv) {
    // 1. Register handler
    std::wstring serviceName = AppConstants::APP_APPLICATION_NAME.toStdWString();
    m_statusHandle = RegisterServiceCtrlHandler(serviceName.c_str(), ServiceCtrlHandler);

    m_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_status.dwServiceSpecificExitCode = 0;

    // 2. Report starting
    updateStatus(SERVICE_START_PENDING);

    // --- INITIALIZATION ---
    QCoreApplication a(m_argc, m_argv);
    m_app = &a;

    // Ensure the service knows it is running as a service
    g_context.isService = true;

    // 3. Create core objects
    Ups_api_library upsCore(m_app);
    UpsMonitorCore monitorService(m_app);
    UpsIpcServer ipcServer(&upsCore, m_app);

    // 4. Connections
    QObject::connect(&ipcServer, &UpsIpcServer::settingsChanged,
                     &monitorService, &UpsMonitorCore::loadSettings);

    QObject::connect(&upsCore, &Ups_api_library::upsReportAvailable,
                     &monitorService, &UpsMonitorCore::handleUpsReport);

    // 5. Start servers
    if (!ipcServer.startServer()) {
        logEvent(QCoreApplication::translate("WindowsService", "Critical: IPC Server could not start."), EVENTLOG_ERROR_TYPE);
        updateStatus(SERVICE_STOPPED);
        return;
    }
    upsCore.startService();

    // 6. Success Log
    logEvent(QCoreApplication::translate("WindowsService", "Service started successfully."));

    // 7. Running
    updateStatus(SERVICE_RUNNING);

    // 8. Event Loop
    m_app->exec();

    // 9. Cleanup
    logEvent(QCoreApplication::translate("WindowsService", "Service has stopped."));
    updateStatus(SERVICE_STOPPED);
}

void WINAPI WindowsService::ServiceCtrlHandler(DWORD ctrl) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
        updateStatus(SERVICE_STOP_PENDING);
        if (m_app) {
            m_app->quit();
        }
        break;
    default:
        break;
    }
}

void WindowsService::updateStatus(DWORD state) {
    m_status.dwCurrentState = state;
    m_status.dwControlsAccepted = (state == SERVICE_RUNNING) ? SERVICE_ACCEPT_STOP : 0;
    m_status.dwWin32ExitCode = NO_ERROR;
    m_status.dwCheckPoint = 0;
    m_status.dwWaitHint = 0;
    SetServiceStatus(m_statusHandle, &m_status);
}

// #ifdef Q_OS_WIN
// void WindowsService::logEvent(const QString &message, WORD type, DWORD eventId) {
//     // Only write to Windows Event Log if the handle exists AND we are in service mode
//     if (g_context.isService && m_statusHandle != nullptr) {
//         std::wstring serviceName = AppConstants::APP_APPLICATION_NAME.toStdWString();
//         HANDLE hEventSource = RegisterEventSource(NULL, serviceName.c_str());
//         if (hEventSource != nullptr) {
//             std::wstring wmsg = message.toStdWString();
//             LPCWSTR strings[1] = { wmsg.c_str() };
//             ReportEvent(hEventSource, type, 0, eventId, NULL, 1, 0, strings, NULL);
//             DeregisterEventSource(hEventSource);
//         }
//     }

//     // Always output to qDebug for the MessageHandler (Console/Debug mode)
//     QString prefix;
//     if (type == EVENTLOG_ERROR_TYPE) prefix = "[CRITICAL]";
//     else if (type == EVENTLOG_WARNING_TYPE) prefix = "[WARNING]";
//     else prefix = "[INFO]";

//     qDebug().noquote() << QString("%1 %2").arg(prefix.leftJustified(10), message);
// }
// #endif
#ifdef Q_OS_WIN
void WindowsService::logEvent(const QString &message, WORD type, DWORD eventId) {
    // 1. Always output to qDebug for console/debug purposes
    QString prefix = "[INFO ]";
    if (type == EVENTLOG_ERROR_TYPE) prefix = "[ERROR]";
    else if (type == EVENTLOG_WARNING_TYPE) prefix = "[WARN ]";

    qDebug().noquote() << QString("%1 %2").arg(prefix, message);

    // 2. Report to Windows Event Log
    // Using the application name as the event source
    HANDLE hEventSource = RegisterEventSourceW(NULL, (LPCWSTR)AppConstants::APP_APPLICATION_NAME.utf16());

    if (hEventSource != NULL) {
        std::wstring wmsg = message.toStdWString();
        LPCWSTR strings[1] = { wmsg.c_str() };

        ReportEventW(
            hEventSource,   // Event log handle
            type,           // Event type
            0,              // Event category
            eventId,        // Event identifier
            NULL,           // User security identifier
            1,              // Number of strings
            0,              // Data size
            strings,        // Array of strings
            NULL            // Binary data
            );
        DeregisterEventSource(hEventSource);
    }
}
#endif

// void WindowsService::registerEventSource() {
// #ifdef Q_OS_WIN
//     // Registry path for Event Log sources
//     QString subKey = QString("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%1")
//                          .arg(AppConstants::APP_APPLICATION_NAME);

//     QSettings settings(subKey, QSettings::NativeFormat);

//     // If the key doesn't exist or isn't configured, set the required values
//     if (!settings.contains("EventMessageFile")) {
//         // Use the current executable path as the message file
//         QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

//         settings.setValue("EventMessageFile", exePath);
//         // Supported types: Error (1), Warning (2), Information (4) -> Sum = 7
//         settings.setValue("TypesSupported", 7);
//         settings.sync();

//         qDebug() << "Registry: Event source registered for" << AppConstants::APP_APPLICATION_NAME;
//     }
// #endif
// }
