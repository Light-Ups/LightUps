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

#include <QString>
#include <QDateTime>
#include <QMetaType>
#include <QObject>

// --- NAMESPACE VOOR ENUMS ---
namespace UpsMonitor {
Q_NAMESPACE

enum class UpsState {
    Unknown,            // Priority 1: Unknown status / IPC Error
    OnlineFull,         // Priority 5: Online, Battery Full/Trickle charging (Vin OK, No Large Current)
    OnlineCharging,     // Priority 4: Online, Battery Charging (Vin OK, Large Current)
    OnlineFault,        // Priority 3: Online, Network error (Vin OK, Bit 2 ON)
    OnBattery,          // Priority 2: Running on Battery (Vin NOT OK)
    BatteryCritical,    // Priority 2: Running on Battery, Critically Low (Bit 1 ON)
};
Q_ENUM_NS(UpsState)
}
// --- END NAMESPACE ---

/**
 * @brief Universal structure for UPS data (combined with critical status).
 */
struct UpsData {
    QDateTime timestamp;
    UpsMonitor::UpsState state = UpsMonitor::UpsState::Unknown; // <--- CHANGE DEFAULT VALUE HERE; // The enumerated status (NEW)
    double inputVoltage =0.0;        // In Volts (V)
    double outputVoltage =0.0;       // In Volts (V)
    double batteryVoltage =0.0;      // In Volts (V)
    double batteryLevel =0.0;        // In percentage (%)
    double temperatureC =0.0;        // In degrees Celsius
    int loadPercentage =0;           // Load in percentage (%)
    bool BatteryFault = false;       // True if the battery needs replacement
    QString statusMessage = "Initialiseren..."; // Provide a clear start value; // Short status (e.g., "OK", "Low Battery")
};

/**
 * @brief Status of the UPS monitoring service (the API layer), including the Driver status.
 */
struct UpsServiceStatus {
    QDateTime timestamp;
    bool driverLoaded = false;          // Was the plugin DLL/SO successfully loaded?
    bool driverInitialized = false;     // Was the initialize() method successfully executed?
    bool dataCommunicationActive = false; // Are dataReceived signals currently being received?
    QString activeDriverName;           // Name of the active driver
    QString activeComPort;              // The port being used
    QString lastErrorMessage;           // The most recent critical error
};

/**
 * @brief The combined reporting structure for all output. This is the structure sent via the signal.
 */
struct UpsReport {
    UpsData data;
    UpsServiceStatus serviceStatus;
};

// Register the types for Qt's signal/slot system
Q_DECLARE_METATYPE(UpsMonitor::UpsState)
Q_DECLARE_METATYPE(UpsData)
Q_DECLARE_METATYPE(UpsServiceStatus)
Q_DECLARE_METATYPE(UpsReport)
