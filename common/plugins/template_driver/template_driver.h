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

#ifndef TEMPLATE_DRIVER_H
#define TEMPLATE_DRIVER_H

#include <QTimer>
#include <QDateTime>
#include "i_ups_driver.h"

/**
 * @brief A safe Template Driver that implements the interface without hardware.
 */
class Template_driver : public IUpsDriver
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IUpsDriver_iid FILE "template_driver.json")
    Q_INTERFACES(IUpsDriver)

public:
    Template_driver(QObject *parent = nullptr);
    virtual ~Template_driver();

    // Interface implementations
    bool initialize(const QString& connectionInfo) override;
    // UpsData fetchData() override;
    QString driverName() const override { return "Template_Mock_Driver"; }

public Q_SLOTS:
    void stopDriver();

private Q_SLOTS:
    void generateMockData();
    void finishInitialization();

private:
    QTimer* m_timer = nullptr;
    UpsData m_latestData;

    // Constants for safe test data
    const double SAFE_VOLTAGE = 230.0;
    const double SAFE_BATTERY = 100.0;
};

#endif // TEMPLATE_DRIVER_H
