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

#include <QApplication>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QStyle>
#include <QDebug>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QWidget>
#include <QCursor>
#include <QLabel>
#include <QSerialPortInfo>
#include <QSettings>
#include "systemtrayapp.h"
#include "constants.h"
#include <QLocale>
#include <QTranslator>

// Global flag for debug output
static bool g_debugEnabled = false;

void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context);

    // Only filter Debug messages, always show the rest (Warning/Critical)
    if (type == QtDebugMsg && !g_debugEnabled) {
        return;
    }
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:    fprintf(stderr, "DEBUG: %s\n", localMsg.constData()); break;
    case QtInfoMsg:     fprintf(stderr, "INFO: %s\n", localMsg.constData()); break;
    case QtWarningMsg:  fprintf(stderr, "WARN: %s\n", localMsg.constData()); break;
    case QtCriticalMsg: fprintf(stderr, "CRIT: %s\n", localMsg.constData()); break;
    case QtFatalMsg:    fprintf(stderr, "FATAL: %s\n", localMsg.constData()); abort();
    }
    // CRUCIAL: Force the data directly to the console
    fflush(stderr);

    if (type == QtFatalMsg) abort();
}

int main(int argc, char *argv[])
{
    QStringList args;
    for(int i = 0; i < argc; ++i) {
        args.append(QString::fromLocal8Bit(argv[i]));
    }
    g_debugEnabled = args.contains("--debug");
    qInstallMessageHandler(myMessageHandler);

    QApplication a(argc, argv);

    a.setQuitOnLastWindowClosed(false);

    QLocale dutch("nl");
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "NobreakMonitor_" + QLocale(locale).name();
        // const QString baseName = "NobreakMonitor_" + dutch.name();
        qDebug() << baseName;
        if (translator.load(":/i18n/" + baseName)) {
            qDebug() << "basename exists for" << baseName;
            a.installTranslator(&translator);
            break;
        }
    }
    QCoreApplication::setOrganizationName(AppConstants::APP_ORGANIZATION_NAME);
    QCoreApplication::setApplicationName(AppConstants::APP_APPLICATION_NAME);
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());

    SystemTrayApp trayApp(&a); // Create an instance of our structured application
    return a.exec();
}
