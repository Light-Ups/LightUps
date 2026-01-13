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

#include "ups_ipc_server.h"
#include <QDataStream>
#include <QDebug>
#include <QCoreApplication>
#include <QSettings>
#include "ipc_constants.h"
#include "constants.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <sddl.h>
#include <accctrl.h>
#include <aclapi.h>
#endif

UpsIpcServer::UpsIpcServer(Ups_api_library* upsCore, QObject *parent)
    : QObject(parent), m_server(new QLocalServer(this))
{
    // Connect the UPS API layer signal to the IPC server's transmission slot.
    connect(upsCore, &Ups_api_library::upsReportAvailable,
            this, &UpsIpcServer::sendReportToClients);
    connect(m_server, &QLocalServer::newConnection, this, &UpsIpcServer::newConnection);
}

UpsIpcServer::~UpsIpcServer()
{
    // Remove the server first so that no new connections are accepted.
    m_server->close();

    // Close and remove all active sockets.
    for (QLocalSocket* socket : std::as_const(m_clients)) {
        socket->abort();
        socket->deleteLater();
    }
    m_clients.clear();
}

bool UpsIpcServer::startServer()
{
    // If an old socket is still active, remove it.
    if (m_server->removeServer(IPC_SERVER_NAME)) {
        qDebug() << "IPC Server: Old server instance removed.";
    }
    if (!m_server->listen(IPC_SERVER_NAME)) {
        qDebug() << "IPC Server: Unable to listen on" << IPC_SERVER_NAME << ":" << m_server->errorString();
        return false;
    }

#ifdef Q_OS_WIN
    // Build the full pipe path for the Windows API
    QString fullPipePath = "\\\\.\\pipe\\" + QString(IPC_SERVER_NAME);

    // SDDL string: 'D:' (DACL), 'A' (Allow), 'GA' (Generic All access), 'S-1-1-0' (Everyone)
    const char* sddl = "D:(A;;GA;;;S-1-1-0)";
    PSECURITY_DESCRIPTOR psd = nullptr;
    if (ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl, SDDL_REVISION_1, &psd, nullptr)) {
        // Adjust the security of the newly created pipe
        if (SetFileSecurityA(fullPipePath.toLocal8Bit().constData(), DACL_SECURITY_INFORMATION, psd)) {
            qDebug() << "IPC Server: Permissions successfully set for Everyone.";
        } else {
            qDebug() << "IPC Server: SetFileSecurity failed. Error:" << GetLastError();
        }
        LocalFree(psd);
    }
#endif
    qDebug() << "IPC Server: Listening started on" << IPC_SERVER_NAME;
    return true;
}

void UpsIpcServer::newConnection()
{
    QLocalSocket* socket = m_server->nextPendingConnection();
    if (socket) {
        qDebug() << "IPC Server: New client connected.";
        m_clients.append(socket);

        // Ensure we know when the client disconnects
        connect(socket, &QLocalSocket::disconnected, this, &UpsIpcServer::socketDisconnected);
        connect(socket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::errorOccurred),
                this, &UpsIpcServer::socketDisconnected);
        connect(socket, &QLocalSocket::readyRead, this, &UpsIpcServer::readyRead);
    }
}

void UpsIpcServer::socketDisconnected()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket) {
        qDebug() << "IPC Server: Client disconnected.";
        m_clients.removeOne(socket);
        socket->deleteLater();
    }
}

void UpsIpcServer::sendReportToClients(const UpsReport& report)
{
    if (m_clients.isEmpty()) return;
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);

    // Reserve space for the size (placeholder)
    out << (quint32)0;

    // Serialize the data directly into the same buffer
    out << report;

    // Go back to the beginning to write the actual size
    out.device()->seek(0);
    out << (quint32)(packet.size() - sizeof(quint32));

    // Send in one go
    for (QLocalSocket* socket : std::as_const(m_clients)) {
        if (socket->state() == QLocalSocket::ConnectedState) {
            socket->write(packet);
        }
    }
}

// De implementatie van readyRead:
void UpsIpcServer::readyRead() {
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_0);

    // 1. First read the size of the packet (the quint32 sent by the GUI)
    static quint32 blockSize = 0;
    if (blockSize == 0) {
        if (socket->bytesAvailable() < sizeof(quint32))
            return; // Wait for more data to be able to read the header
        in >> blockSize;
    }

    // 2. Check if the full payload (the QMap) has arrived
    if (socket->bytesAvailable() < blockSize) {
        return; // Wait until the entire packet is present
    }

    // Reset blockSize for the next command
    blockSize = 0;

    // 3. Now we can safely read the QMap
    QMap<QString, QString> commandData;
    in >> commandData;
    if (in.status() == QDataStream::Ok) {
        processCommand(commandData);
    }
}

// Helper method to keep the logic clean
void UpsIpcServer::processCommand(const QMap<QString, QString>& data) {
    QString command = data.value("COMMAND");
    if (command == "CONFIG_UPDATE") {
        qDebug() << "IPC Server: Config update received.";
        QSettings settings(AppConstants::SETTINGS_SCOPE,
                           AppConstants::APP_ORGANIZATION_NAME,
                           AppConstants::APP_APPLICATION_NAME);

        // We iterate through the map and ignore the "COMMAND" key
        for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
            if (it.key() == "COMMAND") continue;
            settings.setValue(it.key(), it.value());
            qDebug() << "Registry: " << it.key() << " changed to " << it.value();
            qDebug() << "Registry modified via scope:" << AppConstants::SETTINGS_SCOPE
                     << "Key:" << it.key();
        }
        settings.sync();
        emit settingsChanged(); // Tell the rest of the app that the data is fresh
        qDebug() << "IPC Server: Signal settingsChanged emitted.";
    }
}
