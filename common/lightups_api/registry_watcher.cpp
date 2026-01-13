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

#include "registry_watcher.h"
#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include "constants.h"

#ifdef Q_OS_WIN

RegistryWatcher::RegistryWatcher(QObject *parent) : QObject(parent)
{
    // Create a handle that the Windows API can signal
    m_eventHandle = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

void RegistryWatcher::startWatching(const QString& registryPath)
{
    if (m_watching) {
        qDebug() << "RegistryWatcher: Already started.";
        return;
    }

    // Determine the root key based on the scope from constants.h
    HKEY rootKey = (AppConstants::SETTINGS_SCOPE == QSettings::UserScope) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;

    // Build the path
    QString subKey = "Software\\" + AppConstants::APP_ORGANIZATION_NAME + "\\" + AppConstants::APP_APPLICATION_NAME;

    // Use the dynamic rootKey instead of HKEY_CURRENT_USER
    if (RegOpenKeyEx(rootKey, (const wchar_t*)subKey.utf16(), 0, KEY_NOTIFY, &m_hKey) != ERROR_SUCCESS) {
        qDebug() << "RegistryWatcher: Cannot open registry key:" << subKey;
        return;
    }

    m_watching = true;
    qDebug() << "RegistryWatcher: Started monitoring registry changes.";

    // This loop waits asynchronously for changes
    while (m_watching) {

        // Asynchronously notify of changes
        RegNotifyChangeKeyValue(m_hKey, TRUE, REG_NOTIFY_CHANGE_LAST_SET, m_eventHandle, TRUE);

        // Wait until the event occurs or stop is requested
        if (WaitForSingleObject(m_eventHandle, INFINITE) == WAIT_OBJECT_0) {
            if (m_watching) {
                // Event occurred AND we are still watching (not stopped)
                qDebug() << "RegistryWatcher: Settings change detected!";
                emit settingsChanged();
            }
            // Reset the event for the next iteration
            ResetEvent(m_eventHandle);
        }
        // Small sleep; however, WaitForSingleObject already handles blocking until an event occurs.
        QThread::msleep(100);
    }

    if (m_hKey) {
        RegCloseKey(m_hKey);
        m_hKey = nullptr;
    }
    CloseHandle(m_eventHandle);
    m_eventHandle = nullptr;
    qDebug() << "RegistryWatcher: Monitoring stopped.";
}

void RegistryWatcher::stopWatching()
{
    m_watching = false;
    if (m_eventHandle) {
        SetEvent(m_eventHandle); // Interrupt WaitForSingleObject immediately
    }
}

#endif // Q_OS_WIN
