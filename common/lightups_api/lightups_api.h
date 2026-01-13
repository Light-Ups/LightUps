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

#ifndef UPS_API_LIBRARY_H
#define UPS_API_LIBRARY_H

#include "lightups_api_global.h"
#include "i_ups_driver.h"
#include "registry_watcher.h"
#include "ups_report.h"
#include <QObject>
#include <QThread>
#include <QPluginLoader>
#include <QTimer>
#include <QMutex>

class UPS_API_LIBRARY_EXPORT Ups_api_library : public QObject
{
    Q_OBJECT
public:
    explicit Ups_api_library(QObject *parent = nullptr);
    ~Ups_api_library();

    void startService();

Q_SIGNALS:
    void upsReportAvailable(const UpsReport& report);
    void driverInitSuccess();
    void driverInitFailure(const QString& error);

private Q_SLOTS:
    void handleDriverData(const UpsData& data);
    void onDriverInitSuccess();
    void onDriverInitFailure(const QString& error);
    bool loadAndStartDriver(); // Now a slot for safe restarts
    void onRegistryChanged(); // The intermediate step

private:
    void cleanupDriver();
    void emitUpsReport(const UpsData& data = UpsData());

    // Hardware/Driver components
    IUpsDriver *m_driver = nullptr;
    QPluginLoader *m_pluginLoader = nullptr;
    QThread *m_workerThread = nullptr;

    // Monitoring components
    RegistryWatcher *m_watcher = nullptr;
    QThread *m_registryThread = nullptr;
    QTimer *m_recoveryTimer = nullptr;

    // Status & Thread safety
    UpsServiceStatus m_currentStatus;
    QMutex m_cleanupMutex;
};

#endif
