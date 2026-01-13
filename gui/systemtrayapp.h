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

#ifndef SYSTEMTRAYAPP_H
#define SYSTEMTRAYAPP_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QApplication>
#include <QHash>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QDataStream>
#include "upsiconmanager.h"
#include <QIcon>
#include <QPixmap>
#include <QVariant>
#include <QPointer>
#include <QMessageBox>
#include "upsstatuswindow.h"

class SystemTrayApp : public QObject
{
    Q_OBJECT
public:
    explicit SystemTrayApp(QApplication *app, QObject *parent = nullptr);
    ~SystemTrayApp();

private slots:
    void openSmallWindow();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void handleUpsReport(const UpsReport &report);
    void connectToServer();
    void socketConnected();
    void socketDisconnected();
    void socketReadyRead();
    void socketError(QLocalSocket::LocalSocketError socketError);

private:
    QPointer<QMessageBox> m_aboutBox;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QSettings *m_settings;
    QApplication *m_app;
    UpsIconManager *m_iconManager = nullptr;
    QLocalSocket *m_localSocket = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    quint32 m_nextBlockSize = 0;
    UpsStatusWindow *m_statusWindow = nullptr;
    QHash<QString, QJsonObject> m_driverMetadata;
    UpsReport m_lastReport;
    UpsMonitor::UpsState determineRequiredIconStatus() const;
    void updateTrayIconStatus();
    void updateTrayIconTooltip();
    void createTrayIcon();
    void createTrayMenu();
    void loadAvailableDriversMetadata();
    void notifyService(const QString &key, const QString &value);
    void sendFullConfiguration(const QString &driver, const QString &port, int delay, bool powerSafe);
};

#endif // SYSTEMTRAYAPP_H
