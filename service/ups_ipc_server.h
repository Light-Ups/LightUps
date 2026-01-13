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

#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QList>
#include "lightups_api.h"
/**
 * @brief Beheert de lokale server en het verzenden van UpsReport via IPC.
 */
class UpsIpcServer : public QObject
{
    Q_OBJECT
public:
    explicit UpsIpcServer(Ups_api_library* upsCore, QObject *parent = nullptr);
    ~UpsIpcServer();

public slots:
    bool startServer();
    void readyRead();

private slots:
    void newConnection();
    void socketDisconnected();
    void sendReportToClients(const UpsReport& report);

private:
    QLocalServer *m_server;
    QList<QLocalSocket*> m_clients;
    void processCommand(const QMap<QString, QString>& data);

signals:
    void settingsChanged();
};
