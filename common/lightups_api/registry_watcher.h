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

#ifndef REGISTRY_WATCHER_H
#define REGISTRY_WATCHER_H

#include <QObject>

// Only required for Windows functionality
#ifdef Q_OS_WIN
#include <qt_windows.h>

class RegistryWatcher : public QObject
{
    Q_OBJECT
public:
    explicit RegistryWatcher(QObject *parent = nullptr);

public Q_SLOTS:
    void startWatching(const QString& registryPath);
    void stopWatching();

Q_SIGNALS:
    void settingsChanged();

private:
    std::atomic<bool> m_watching = false;
    HANDLE m_eventHandle = nullptr;
    HKEY m_hKey = nullptr;
};

#endif // Q_OS_WIN
#endif // REGISTRY_WATCHER_H
