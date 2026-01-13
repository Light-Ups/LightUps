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

#ifndef UPSICONMANAGER_H
#define UPSICONMANAGER_H

#include <QObject>
#include <QSvgRenderer>
#include <QIcon>
#include <QSize>
#include <QDomDocument>
#include "ups_report.h"

class UpsIconManager : public QObject
{
    Q_OBJECT
public:
    explicit UpsIconManager(QObject *parent = nullptr);
    QIcon getIconForStatus(UpsMonitor::UpsState status, const QSize& baseSize = QSize(16, 16));

private:
    QSvgRenderer *m_svgRenderer = nullptr;
    QDomDocument m_svgDocument;

    // Static helper function to find an element by ID
    static QDomElement findSvgElementById(const QDomElement &root, const QString &id);
    void setElementDisplay(const QString& id, bool visible);
    void configureSvgLayers(UpsMonitor::UpsState status);
    QPixmap renderSvgToPixmap(const QSize& size);
};

#endif // UPSICONMANAGER_H
