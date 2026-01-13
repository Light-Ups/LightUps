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
#include <QTimer>
#include "ups_report.h"

class UpsMonitorCore : public QObject
{
    Q_OBJECT
public:
    explicit UpsMonitorCore(QObject *parent = nullptr);

public slots:
    void handleUpsReport(const UpsReport &report);
    void loadSettings();

private slots:
    void executeShutdown();
    void restoreCpuSpeed();

private:
    QTimer *m_shutdownTimer;
    QTimer *m_cpuRecoveryTimer;
    bool m_isTimerRunning;
    UpsMonitor::UpsState m_lastState;
    bool m_currentPowerModeIsBattery; // Keeps track of the current Windows state
    void setPowerMode(bool batteryMode);
    void checkAndFixPowerProfile();
    int m_shutdownDelay;      // Stores the current delay
    bool m_powerSafeEnabled;  // Stores the powersafe status
    void initializeRegistry();
};
