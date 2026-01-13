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

#include "template_driver.h"
#include <QDebug>

Template_driver::Template_driver(QObject *parent)
    : IUpsDriver(parent)
{
    // We initialize the data with safe values
    m_latestData.state = UpsMonitor::UpsState::Unknown;
    m_latestData.inputVoltage = 0;
    m_latestData.statusMessage = tr("Initializing...");
}

Template_driver::~Template_driver()
{
    qDebug() << "Template_driver: Destructor called.";
}

bool Template_driver::initialize(const QString& connectionInfo)
{
    qDebug() << "Template_driver: Initializing on port:" << connectionInfo;

    // Setup timer for data simulation
    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &Template_driver::generateMockData);
    }

    // Simulate a short delay for a 'handshake' (e.g. 1 second)
    QTimer::singleShot(1000, this, &Template_driver::finishInitialization);

    return true; // Indicates that the setup succeeded
}

void Template_driver::finishInitialization()
{
    qDebug() << "Template_driver: Handshake successful.";

    // Start the driver heartbeat (data every second)
    m_timer->start(1000);

    // Notify the API that the driver is ready for use
    emit initializationSuccess();
}

void Template_driver::generateMockData()
{
    // Fill the struct with guaranteed safe values
    m_latestData.timestamp = QDateTime::currentDateTime();
    m_latestData.state = UpsMonitor::UpsState::OnlineFull; // ALWAYS SAFE
    m_latestData.inputVoltage = SAFE_VOLTAGE;
    m_latestData.outputVoltage = SAFE_VOLTAGE;
    m_latestData.batteryVoltage = 13.6;
    m_latestData.batteryLevel = SAFE_BATTERY;
    m_latestData.temperatureC = 25.0;
    m_latestData.loadPercentage = 15;
    m_latestData.BatteryFault = false;
    m_latestData.statusMessage = tr("Template Mode: System OK");

    // Send to the API layer
    emit dataReceived(m_latestData);
}

// UpsData Template_driver::fetchData()
// {
//     return m_latestData;
// }

void Template_driver::stopDriver()
{
    qDebug() << "Template_driver: Stopping...";
    if (m_timer) {
        m_timer->stop();
    }
}
