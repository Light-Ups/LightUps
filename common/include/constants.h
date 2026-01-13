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

#ifndef CONSTANTS_H
#define CONSTANTS_H

// #define QT_DEBUG

#include <QString>
#include <QSettings>
#include <windows.h>

namespace AppConstants {

// --- DEBUG SWITCH ---
// Set this to false if you really want to test service permissions (HKLM)
#ifdef QT_DEBUG
const QSettings::Scope SETTINGS_SCOPE = QSettings::UserScope;   // HKEY_CURRENT_USER
#else
const QSettings::Scope SETTINGS_SCOPE = QSettings::SystemScope; // HKEY_LOCAL_MACHINE
#endif

// -------------------------------------------------------------------------
// QSettings (Windows Registry) Keys
// HKEY_CURRENT_USER\Software\MijnOrganisatie\MijnTrayApp
// -------------------------------------------------------------------------

// Key for the last selected COM port (String)
const QString REG_KEY_SELECTED_COM_PORT = "SelectedComPort";

// Key for the last selected driver Filename (String)
const QString REG_KEY_SELECTED_DRIVER_FILE = "SelectedDriver";

// Key for the shutdown delay in seconds (Int)
const QString REG_KEY_SHUTDOWN_DELAY = "ShutdownDelay";

// Key for the Power Safe Mode checkbox (Bool)
const QString REG_KEY_POWER_SAFE_ENABLED = "PowerSafeEnabled";

// -------------------------------------------------------------------------
// Application & Organization Names (QCoreApplication::set*)
// -------------------------------------------------------------------------

// Organization name that determines the path in the Windows Registry
const QString APP_ORGANIZATION_NAME = "andhoo";

// Application name that determines the path in the Windows Registry
const QString APP_APPLICATION_NAME = "LightUps";
}

// namespace AppConstants
namespace UpsEvents {
const DWORD ID_SERVICE_INFO    = 100; // Start, Stop, Settings change
const DWORD ID_POWER_RESTORED  = 200; // AC restored
const DWORD ID_ON_BATTERY      = 300; // AC lost (Warning)
const DWORD ID_BATT_CRITICAL   = 400; // System is shutting down (Error)
const DWORD ID_SERVICE_ERROR   = 900; // Internal errors (e.g. IPC server fails)
}

struct AppContext {
    bool debugMode = false;
    bool consoleMode = false;
    bool isService = false;
};

extern AppContext g_context;

#endif // CONSTANTS_H
