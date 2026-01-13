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

#include "upsiconmanager.h"
#include <QPainter>
#include <QDebug>
#include <QFile>
#include <QCoreApplication> // Required for QDomElement::attribute

UpsIconManager::UpsIconManager(QObject *parent)
    : QObject(parent) // <<< Only parent in the list
{
    qDebug() << "DEBUG: UpsIconManager constructor reached."; // <<< FIRST CHECK

    // Initialize the renderer now in the body
    m_svgRenderer = new QSvgRenderer(this);
    QFile file(":/assets/ups_status.svg");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Error A: Could NOT open the SVG file from resources at path ':/assets/ups_status.svg'.";
        return;
    }
    QByteArray svgData = file.readAll();
    file.close();

    // Use the new overload that returns a QDomDocument::ParseResult
    auto result = m_svgDocument.setContent(svgData);
    if (!result) {
        qDebug() << QString("Error B: FATAL XML ERROR on line %1, column %2: %3")
        .arg(result.errorLine)
            .arg(result.errorColumn)
            .arg(result.errorMessage);
        return;
    }

    // Everything loaded. We are ready.
    qDebug() << "DEBUG: UpsIconManager SVG successfully loaded and parsed.";
}

// =========================================================================
// Static Helper Function: Recursively Search for Element by ID
// =========================================================================

QDomElement UpsIconManager::findSvgElementById(const QDomElement &root, const QString &id)
{
    // 1. Check if the current element has the searched ID
    if (root.attribute("id") == id) {
        return root;
    }

    // 2. Traverse all child elements recursively
    QDomNode node = root.firstChild();
    while (!node.isNull()) {
        if (node.isElement()) {
            QDomElement found = findSvgElementById(node.toElement(), id);
            if (!found.isNull()) {
                return found;
            }
        }
        node = node.nextSibling();
    }
    return QDomElement(); // Element not found in this branch
}

// =========================================================================
// Change in setElementDisplay
// =========================================================================

void UpsIconManager::setElementDisplay(const QString& id, bool visible)
{
    QDomElement root = m_svgDocument.documentElement();
    QDomElement element = findSvgElementById(root, id);

    if (!element.isNull()) {
        QString currentStyle = element.attribute("style");
        QString newDisplayValue = visible ? "inline" : "none";

        // 1. Remove the separate 'display' attribute, as it conflicts.
        element.removeAttribute("display");

        // 2. Search and replace the 'display:...' property in the style string.
        // Search for the existing 'display:' pattern in the style string
        int displayIndex = currentStyle.indexOf("display:");

        if (displayIndex != -1) {
            // A 'display:' property already exists. Find the end (semicolon).
            int semicolonIndex = currentStyle.indexOf(';', displayIndex);
            if (semicolonIndex != -1) {
                // Replace the old display value, including the semicolon
                currentStyle.replace(displayIndex, semicolonIndex - displayIndex + 1, QString("display:%1;").arg(newDisplayValue));
            } else {
                // If there is no semicolon (last attribute), replace until the end of the string.
                // This is safer than a Regex.
                currentStyle.replace(displayIndex, currentStyle.length() - displayIndex, QString("display:%1").arg(newDisplayValue));
            }
        } else {
            // There is NO 'display:' property in the style string. Add it.
            if (!currentStyle.isEmpty() && !currentStyle.endsWith(";")) {
                currentStyle += ";";
            }
            currentStyle += QString("display:%1;").arg(newDisplayValue);
        }

        // 3. Write the NEW style string back to the attribute
        element.setAttribute("style", currentStyle);
    } else {
        qDebug() << "Warning: SVG element with ID" << id << "not found.";
    }
}

// -------------------------------------------------------------------------
// 1. Configuration of the Layers
// -------------------------------------------------------------------------

// Declare alias

void UpsIconManager::configureSvgLayers(UpsMonitor::UpsState status)
{
    using enum UpsMonitor::UpsState;
    // Use the new setElementDisplay method!

    // Reset: Hide all variable indicators and fill colors
    setElementDisplay("id_cross", false);
    setElementDisplay("id_exclamation", false);
    setElementDisplay("id_flash", false);
    setElementDisplay("id_plug", false);
    setElementDisplay("id_cable", false);
    setElementDisplay("id_no_sync", false);
    setElementDisplay("id_fill_yellow", false);
    setElementDisplay("id_fill_green", false);
    setElementDisplay("id_fill_red", false);
    setElementDisplay("id_battery_outline", true);
    switch (status) {
    case OnlineFull:
        setElementDisplay("id_fill_green", true);
        setElementDisplay("id_plug", true);
        setElementDisplay("id_cable", true);
        break;
    case OnlineCharging:
        setElementDisplay("id_fill_green", true);
        setElementDisplay("id_plug", true);
        setElementDisplay("id_cable", true);
        setElementDisplay("id_flash", true);
        break;
    case OnlineFault:
        setElementDisplay("id_fill_yellow", true);
        // setElementDisplay("id_cross", true);
        setElementDisplay("id_plug", true);
        setElementDisplay("id_no_sync", true);
        break;
    case OnBattery:
        setElementDisplay("id_fill_green", true);
        setElementDisplay("id_exclamation", true);
        break;
    case BatteryCritical:
        setElementDisplay("id_fill_yellow", true);
        setElementDisplay("id_exclamation", true);
        break;
    case Unknown:
    default:
        setElementDisplay("id_cross", true);
        // For unknown status, show only the empty battery (gray outline)
        break;
    }
}

// -------------------------------------------------------------------------
// 2. Helper Method: Rendering to QPixmap
// -------------------------------------------------------------------------

QPixmap UpsIconManager::renderSvgToPixmap(const QSize& size)
{
    // 1. Ensure that the renderer and the size are valid.
    // m_svgRenderer->isValid() can still be false at this point.

    if (size.isEmpty() || !m_svgRenderer) {
        qDebug() << "Error: Renderer is invalid or size is empty.";
        return QPixmap();
    }

    // 2. Convert the *modified* QDomDocument to QByteArray
    // This is the crucial step to reflect the modified 'display' attributes.
    QByteArray currentSvgData = m_svgDocument.toByteArray();

    // 3. Load the updated data into the renderer.
    // This is the Qt6 method to synchronize the renderer with the DOM changes.
    if (!m_svgRenderer->load(currentSvgData)) {
        qDebug() << "FATAL RENDER ERROR: Could not load the modified SVG data for rendering.";
        return QPixmap();
    }

    // Check if the load operation succeeded
    if (!m_svgRenderer->isValid()) {
        qDebug() << "Error: QSvgRenderer is invalid after loading the modified data.";
        return QPixmap();
    }

    // 4. Rendering
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    m_svgRenderer->render(&painter);
    return pixmap;
}

// -------------------------------------------------------------------------
// 3. Public Interface: Generating QIcon
// -------------------------------------------------------------------------

QIcon UpsIconManager::getIconForStatus(UpsMonitor::UpsState status, const QSize& baseSize)
{
    if (m_svgDocument.isNull()) {
        return QIcon();
    }

    // Step 1: Configure the SVG to display the correct status
    configureSvgLayers(status);
    QIcon finalIcon;

    // Step 2: Add multiple, scaled versions to the QIcon for the systray
    // This ensures sharpness on different DPI/scales (Windows)

    // Normal sizes (1x)
    finalIcon.addPixmap(renderSvgToPixmap(baseSize));                      // 16x16
    finalIcon.addPixmap(renderSvgToPixmap(QSize(baseSize.width() + 8, baseSize.height() + 8))); // 24x24 (vaak gebruikt)

    // HiDPI sizes (2x)
    finalIcon.addPixmap(renderSvgToPixmap(baseSize * 2));                 // 32x32
    finalIcon.addPixmap(renderSvgToPixmap(QSize(48, 48)));                // 48x48
    return finalIcon;
}
