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

#ifndef UPSSTATUSWINDOW_H
#define UPSSTATUSWINDOW_H

#include <QWidget>
#include "ups_report.h"

namespace Ui { class UpsStatusWindow; }

class UpsStatusWindow : public QWidget
{
    Q_OBJECT

public:
    explicit UpsStatusWindow(QWidget *parent = nullptr);
    ~UpsStatusWindow();
    void resetLabels();
    void addComPort(const QString &portName);
    // Method to populate the UI with available drivers
    void setAvailableDrivers(const QHash<QString, QJsonObject> &driverMetadata);

signals:
    void configurationChanged(const QString &driverFile, const QString &portName, int delay, bool powerSafe);
    void configurationUpdateRequested(const QString &driverFile, const QString &portNameme, int delay, bool powerSafe);

public slots:
    void updateReport(const UpsReport &report);
    void saveSettings();
    void loadSettings();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::UpsStatusWindow *ui;
    QString upsStateToString(UpsMonitor::UpsState state) const;
    void validateSettings();
    // QTimer *m_portScanTimer;
    QHash<QString, QJsonObject> m_driverMetadata;
};

#endif
