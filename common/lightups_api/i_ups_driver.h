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
#include <QtPlugin>
#include "ups_report.h" // <<< CHANGE: Inclusion of the new combined struct
#include "lightups_api_global.h"

/**
 * @brief The pure abstract interface for all UPS drivers/plugins.
 */
class UPS_API_LIBRARY_EXPORT IUpsDriver: public  QObject // <<< WIJZIGING: public virtual QObject
{
Q_OBJECT // ESSENTIAL: Tells MOC that this is a MOC class
public:
    // CHANGE: Add an explicit constructor
    explicit IUpsDriver(QObject *parent = nullptr) {}

    virtual ~IUpsDriver() = default;

    /**
     * @brief Initializes communication with the UPS.
     */
    virtual bool initialize(const QString& connectionInfo) = 0;

    /**
     * @brief Returns the name of the driver.
     */
    virtual QString driverName() const = 0;

Q_SIGNALS: // <--- THE NEW SECTION
    /**
     * @brief Emitted when the driver is successfully initialized.
     */
    void initializationSuccess();

    /**
     * @brief Emitted on a fatal error (startup error or runtime error such as USB disconnection).
     */
    void initializationFailure(const QString& error);
    void dataReceived(const UpsData& data); // This is the data signal.
};

// Register the interface ID for Qt's plugin system
#define IUpsDriver_iid "com.yourcompany.UpsMonitoring.IUpsDriver/1.0"
Q_DECLARE_INTERFACE(IUpsDriver, IUpsDriver_iid)

