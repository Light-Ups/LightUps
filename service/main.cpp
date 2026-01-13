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

#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QStringList>

#include "lightups_api.h"
#include "ups_ipc_server.h"
#include "constants.h"
#include "ups_monitor_service.h"
#include "windows_service.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Initialize the global context defined in constants.h
AppContext g_context;

/**
 * @brief Custom message handler to format output and filter based on debug flag.
 */
void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context);

    // If debug mode is off, suppress debug messages
    if (type == QtDebugMsg && !g_context.debugMode) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QByteArray localMsg = msg.toLocal8Bit();

    const char* typeStr = "INFO";
    switch (type) {
    case QtDebugMsg:    typeStr = "DEBUG"; break;
    case QtWarningMsg:  typeStr = "WARN "; break;
    case QtCriticalMsg: typeStr = "ERROR"; break;
    case QtFatalMsg:    typeStr = "FATAL"; break;
    default:            typeStr = "INFO "; break;
    }

    // Print to stderr for console visibility
    fprintf(stderr, "[%s] %s: %s\n",
            timestamp.toUtf8().constData(),
            typeStr,
            localMsg.constData());
    fflush(stderr);
}

int main(int argc, char *argv[]) {
    // 1. Parse command line arguments
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args.append(QString::fromLocal8Bit(argv[i]));
    }

    // 2. Fill the global context based on arguments
    g_context.consoleMode = args.contains("--console") || args.contains("-c");
    g_context.debugMode   = args.contains("--debug");

    // On Windows, if we don't force console mode, we assume we should try to run as a Service
    g_context.isService   = !g_context.consoleMode;

    // 3. Install the custom logger
    qInstallMessageHandler(myMessageHandler);

    if (g_context.isService) {
        // --- SERVICE MODE ---
        // The Windows Service Control Manager (SCM) will call ServiceMain
        return WindowsService::run(argc, argv);
    }
    else {
        // --- CONSOLE / DEBUG MODE ---
        QCoreApplication a(argc, argv);
        QCoreApplication::setOrganizationName(AppConstants::APP_ORGANIZATION_NAME);
        QCoreApplication::setApplicationName(AppConstants::APP_APPLICATION_NAME);

        WindowsService::logEvent("Application starting in CONSOLE MODE.", EVENTLOG_INFORMATION_TYPE);

        // Initialize core components
        Ups_api_library upsCore(&a);
        UpsMonitorCore monitorService(&a);
        UpsIpcServer ipcServer(&upsCore, &a);

        // Connect components
        QObject::connect(&ipcServer, &UpsIpcServer::settingsChanged,
                         &monitorService, &UpsMonitorCore::loadSettings);

        QObject::connect(&upsCore, &Ups_api_library::upsReportAvailable,
                         &monitorService, &UpsMonitorCore::handleUpsReport);

        // Start the IPC server for communication with the GUI client
        if (!ipcServer.startServer()) {
            WindowsService::logEvent("Critical failure: Could not start IPC server.", EVENTLOG_ERROR_TYPE);
            return -1;
        }
        upsCore.startService();

        return a.exec();
    }
}
