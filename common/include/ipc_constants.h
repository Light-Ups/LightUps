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

#include "ups_report.h" // Necessary to know the structure
#include <QDataStream>
#include <QString>
#include <QMetaEnum>
#include <QDebug> // <<< ADDED

// --- PLACE THIS HERE SO IT IS VISIBLE TO ALL FILES ---
#define IPC_TEST_DEBUG // The definition is now visible everywhere

// The unique name for the local socket/server (must be the same for both apps)
const QString IPC_SERVER_NAME = "Global\\UPS_MONITOR_SERVICE_V1";

/**
 * @brief Helper function to serialize a UpsReport structure to a QDataStream.
 * @param stream The output data stream.
 * @param report The UpsReport structure.
 * @return The output data stream.
 *
 * IMPORTANT: The order in which fields are written,
 * must EXACTLY match the order in which they are later read
 * by the GUI (read: '>>' operator in the GUI).
 */
inline QDataStream& operator<<(QDataStream& stream, const UpsReport& report)
{
#ifdef IPC_TEST_DEBUG
    // Log the details of the sent report.
    qDebug() << "IPC DEBUG: Rapport verzonden - Tijd:" << report.data.timestamp.toString("hh:mm:ss")
             << "| Status:" << (int)report.data.state
             << "| Batterij:" << report.data.batteryLevel << "%";
#endif
    // First the UpsData
    stream << report.data.timestamp;
    stream << (qint32)report.data.state; // Store Enum as integer
    stream << report.data.inputVoltage;
    stream << report.data.outputVoltage;
    stream << report.data.batteryVoltage;
    stream << report.data.batteryLevel;
    stream << report.data.temperatureC;
    stream << report.data.loadPercentage;
    stream << report.data.BatteryFault;
    stream << report.data.statusMessage;

    // Then the UpsServiceStatus
    stream << report.serviceStatus.timestamp;
    stream << report.serviceStatus.driverLoaded;
    stream << report.serviceStatus.driverInitialized;
    stream << report.serviceStatus.dataCommunicationActive;
    stream << report.serviceStatus.activeDriverName;
    stream << report.serviceStatus.activeComPort;
    stream << report.serviceStatus.lastErrorMessage;

    return stream;
}

/**
 * @brief Helper function to deserialize a UpsReport structure from a QDataStream.
 * @param stream The input data stream.
 * @param report The UpsReport structure being filled.
 * @return The input data stream.
 *
 * IMPORTANT: The order in which fields are read,
 * must EXACTLY match the order in which they were written
 * by the Service (read: '<<' operator in the Service).
 */
using UpsMonitor::UpsState;
inline QDataStream& operator>>(QDataStream& stream, UpsReport& report)
{
    qint32 stateInt; // Use a qint32 to read the enum

    // First the UpsData
    stream >> report.data.timestamp;
    stream >> stateInt;
    report.data.state = (UpsState)stateInt; // Cast back to the enum
    stream >> report.data.inputVoltage;
    stream >> report.data.outputVoltage;
    stream >> report.data.batteryVoltage;
    stream >> report.data.batteryLevel;
    stream >> report.data.temperatureC;
    stream >> report.data.loadPercentage;
    stream >> report.data.BatteryFault;
    stream >> report.data.statusMessage;

    // Then the UpsServiceStatus
    stream >> report.serviceStatus.timestamp;
    stream >> report.serviceStatus.driverLoaded;
    stream >> report.serviceStatus.driverInitialized;
    stream >> report.serviceStatus.dataCommunicationActive;
    stream >> report.serviceStatus.activeDriverName;
    stream >> report.serviceStatus.activeComPort;
    stream >> report.serviceStatus.lastErrorMessage;

#ifdef IPC_TEST_DEBUG
QString upsStatusName = "UNKNOWN";

    QMetaEnum metaEnum = QMetaEnum::fromType<UpsState>();

    if (metaEnum.isValid()) {
        // Convert the scoped enum to int for valueToKey().
        upsStatusName = metaEnum.valueToKey(static_cast<int>(report.data.state));
    }

    // Log the details of the received report.
    if (stream.status() == QDataStream::Ok) {
        qDebug() << "IPC DEBUG: Report received - Time:" << report.data.timestamp.toString("hh:mm:ss")
        << "| Status:" << upsStatusName
        << "| Battery:" << report.data.batteryLevel << "%";
    } else {
        qDebug() << "IPC DEBUG: Error during deserialization of UpsReport.";
    }
#endif

    return stream;
}
