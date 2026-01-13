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

#include "nhs_driver.h"
#include <QDateTime>
#include <algorithm>
#include <QThread>
#include <cmath>

// Constants
namespace {
    // Function to safely retrieve commands (Lazy Initialization)
    const QByteArray& getCommandS() { static const QByteArray cmd = QByteArray::fromHex("FF0953030000005FFE"); return cmd; }
    // const QByteArray& getCommandD() { static const QByteArray cmd = QByteArray::fromHex("FF09440300000062FE"); return cmd; }
}

const int PACKET_LEN_D = 21; // Total length FF..FE
const int PACKET_LEN_S = 18; // Total length FF..FE

// ----------------------------------------------------
// --- CONSTRUCTOR AND INTERFACE FUNCTIONS ---
// ----------------------------------------------------
Nhs_driver::Nhs_driver(QObject *parent)
    // The concrete class must initialize the direct base class (IUpsDriver)
    // <<< This calls the new IUpsDriver constructor
    : IUpsDriver(parent)
{
    // Keep the constructor EMPTY! Everything must happen in initialize().
    // Member pointers are now initialized to nullptr.
}

Nhs_driver::~Nhs_driver()
{
    qDebug() << "Nhs_driver: Destructor executing on thread:" << QThread::currentThreadId();
    // No stopDriver() or closePort() here! That has already happened in the cleanup.
    // Qt cleans up m_serialPort and m_monitorTimer automatically because they have 'this' as parent.
}

bool Nhs_driver::initialize(const QString& connectionInfo)
{
    m_portName = connectionInfo; // Save the name (e.g., "COM7")

    // Create objects if they do not yet exist
    if (!m_serialPort) {
        m_serialPort = new QSerialPort(this);
        m_serialPort->setReadBufferSize(128);
    }
    if (!m_monitorTimer) {
        m_monitorTimer = new QTimer(this);
        connect(m_monitorTimer, &QTimer::timeout, this, &Nhs_driver::onMonitorTimeout, Qt::DirectConnection);
    }

    // Serial port settings
    m_serialPort->setBaudRate(QSerialPort::Baud2400);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    // Connections (establish only once)
    disconnect(m_serialPort, nullptr, this, nullptr); // Prevent duplicate connections
    connect(m_serialPort, &QSerialPort::readyRead, this, &Nhs_driver::readData, Qt::DirectConnection);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, &Nhs_driver::handleSerialError, Qt::DirectConnection);

    m_handshakeComplete = false;
    m_retryCount = 0;

    // Try to open the port directly
    if (!tryOpenPort()) {
        qWarning() << "Nhs_driver: Port not directly available. Starting recovery mode...";
    }

    // ALWAYS start the monitor timer (this is our heartbeat for recovery)
    m_monitorTimer->start(3000);

    // We ALWAYS return true to the API, so the driver thread keeps running
    return true;
}

QString Nhs_driver::driverName() const {
    return "NHS_UPS_Driver";
}

/**
 * @brief Calculates the estimated battery charge (%) based on the measured resting voltage (unloaded).
 *
 * Based on the specifications provided by the user for a 12V lead-gel battery:
 * 12.8 V = 100%
 * 11.8 V = ~50%
 * 10.5 V = 0%
 *
 * @param voltage The unloaded battery voltage in Volts..
 * @return The estimated battery charge as an integer percentage (0-100).
 */
int Nhs_driver::calculateBatteryLevelFromVoltage(double voltage) const
{
    // The critical voltage points (Resting voltage) according to specifications
    const double VOLTAGE_100_PERCENT = 12.8;
    const double VOLTAGE_50_PERCENT  = 11.8;
    const double VOLTAGE_0_PERCENT   = 10.5;

    double calculatedLevel = 0.0;

    // --- 1. Below the minimum threshold (0%) ---
    if (voltage <= VOLTAGE_0_PERCENT) {
        return 0;
    }

    // --- 2. Between 0% and 50% (10.5V to 11.8V) ---
    if (voltage <= VOLTAGE_50_PERCENT) {
        double voltageRange = VOLTAGE_50_PERCENT - VOLTAGE_0_PERCENT; // 1.3 V
        double percentRange = 50.0;

        // Interpolation: 50% * (voltage - 10.5) / 1.3
        calculatedLevel = percentRange * (voltage - VOLTAGE_0_PERCENT) / voltageRange;
    }

    // --- 3. Between 50% and 100% (11.8V to 12.8V) ---
    else if (voltage < VOLTAGE_100_PERCENT) {
        double voltageRange = VOLTAGE_100_PERCENT - VOLTAGE_50_PERCENT; // 1.0 V
        double percentRange = 50.0;

        // Interpolation: 50% + 50% * (voltage - 11.8) / 1.0
        calculatedLevel = 50.0 + percentRange * (voltage - VOLTAGE_50_PERCENT) / voltageRange;
    }

    // --- 4. Above the maximum threshold (100%) ---
    else {
        // This catches voltages of 12.8V and higher (such as during float/bulk charging)
        return 100;
    }

    // Ensure the result is between 0 and 100 and is rounded
    return (int)std::round(std::max(0.0, std::min(100.0, calculatedLevel)));
}

// ----------------------------------------------------
// --- HELPER FUNCTIONS FROM main.c ---
// ----------------------------------------------------

quint8 Nhs_driver::calculate_checksum_ring(int tail_index, int length) {
    quint16 sum = 0;
    // We skip the 0xFF (index 0 of packet), so we start at i = 1
    for (int i = 1; i <= length; i++) {
        int ringIndex = (tail_index + i) & BUFFER_MASK;
        sum += m_ringBuffer[ringIndex];
    }
    return (quint8)(sum & 0xFF);
}

UpsData Nhs_driver::convertRawToUpsData() {
    using namespace UpsMonitor;
    UpsData data;
    data.timestamp = QDateTime::currentDateTime();
    data.inputVoltage = m_latestRawData.input_voltage_v;
    data.outputVoltage = m_latestRawData.output_voltage_v;
    data.batteryVoltage = m_latestRawData.battery_voltage_v;
    data.temperatureC = m_latestRawData.temperature_c;

    // --- FIX: Correct assignment of Load (%) ---
    data.loadPercentage = std::max(0, std::min(100, (int)m_latestRawData.power_rms_percent));

    int calculatedLevel = 0;
    data.batteryLevel = (double)calculatedLevel;
    data.BatteryFault = m_latestRawData.s_battery_low; // We use 'low' as 'fault' here, as before.
    const quint8 statusVal = m_latestRawData.payload.statusval;

    // 1. Critical (Bit 1)
    if (statusVal & NhsStatusBits::BATTERY_LOW_CRITICAL) {
        data.state = UpsState::BatteryCritical;
        data.statusMessage = tr("CRITICAL: Battery Low. Shutdown required.");
    }
    // 2. On Battery (Mains Power Lost: !Bit 4)
    else if (!(statusVal & NhsStatusBits::BATTERY_CHARGING)) {
        data.state = UpsState::OnBattery;
        data.statusMessage = tr("On Battery (Power Outage).");
    }
    // 3. Online Fault (Network Error: Bit 2)
    else if (statusVal & NhsStatusBits::FREQUENCY_ASYNC) {
        data.state = UpsState::OnlineFault;
        data.statusMessage = tr("Warning: Input Problem/Network Error.");
    }
    // 4. Actively Charging (Online AND Large Current: Bit 7)
    else if (statusVal & NhsStatusBits::BATTERY_FLOW_ACTIVE) {
        data.state = UpsState::OnlineCharging;
        data.statusMessage = tr("Battery is Actively Charging.");
    }
    // 5. Online Full (Lowest Priority)
    else {
        data.state = UpsState::OnlineFull;
        data.statusMessage = tr("Online (AC OK, Battery Full/Trickle Charging).");
    }
    return data;
}

bool Nhs_driver::parse_packet_ring(int tail_index, int packetLen) {
    // Use a small buffer on the stack for linear access. This is much faster than a QByteArray on the heap and solves the wrap-around problem.
    uint8_t linearPacket[32];
    for (int i = 0; i < packetLen; ++i) {
        linearPacket[i] = m_ringBuffer[(tail_index + i) & BUFFER_MASK];
    }

    quint8 packet_type = linearPacket[2];          // Index 2 is always the type ('D' or 'S')
    const uint8_t* payload_ptr = &linearPacket[3]; // Payload begins at index 3
    bool success = false;

    if (packet_type == 'D' && packetLen == PACKET_LEN_D) {
        // Realtime Status (Type 'D')
        memcpy((void *)&m_latestRawData.payload, payload_ptr, sizeof(nhs_data_payload_t));

        // Bit-shifting and conversion to readable values
        m_latestRawData.input_voltage_v = (quint16)m_latestRawData.payload.vacinrms_low | ((quint16)m_latestRawData.payload.vacinrms_high << 8);
        m_latestRawData.output_voltage_v = (quint16)m_latestRawData.payload.vacoutrms_low | ((quint16)m_latestRawData.payload.vacoutrms_high << 8);
        m_latestRawData.battery_voltage_v = ((quint16)m_latestRawData.payload.vdcmed_low | ((quint16)m_latestRawData.payload.vdcmed_high << 8)) / 10.0f;
        m_latestRawData.temperature_c = (quint16)m_latestRawData.payload.tempmed_low | ((quint16)m_latestRawData.payload.tempmed_high << 8);
        m_latestRawData.power_rms_percent = m_latestRawData.payload.potrms;

        quint8 status_byte = m_latestRawData.payload.statusval;

        // Reading status bits
        m_latestRawData.s_battery_mode = (status_byte & (1 << 0));
        m_latestRawData.s_battery_low = (status_byte & (1 << 1));
        m_latestRawData.s_network_failure = (status_byte & (1 << 2));
        m_latestRawData.s_fast_network_failure = (status_byte & (1 << 3));
        m_latestRawData.s_220_in = (status_byte & (1 << 4));
        m_latestRawData.s_220_out = (status_byte & (1 << 5));
        m_latestRawData.s_bypass_on = (status_byte & (1 << 6));
        m_latestRawData.s_charger_on = (status_byte & (1 << 7));

        qDebug() << "Type D parsed. Input:" << m_latestRawData.input_voltage_v << "V, "
                 << "Output:" << m_latestRawData.output_voltage_v << "V, "
                 << "Battery:" << m_latestRawData.battery_voltage_v << "V, "
                 << "status:" << status_byte;
        success = true;

        if (m_handshakeComplete) {
            m_monitorTimer->start(MONITOR_TIMEOUT); // Reset watchdog
            if (!m_initialSDataReceived) {
                m_initialSDataReceived = true;
                qDebug() << "Nhs_driver: First valid data (D-record) received via ring buffer.";
            }
        }
    }
    else if (packet_type == 'S' && packetLen == PACKET_LEN_S) {
        // Hardware Info (Type 'S')
        memcpy((void *)&m_latestRawData.hardware_payload, payload_ptr, sizeof(nhs_hardware_payload_t));
        m_latestRawData.uv_220v = m_latestRawData.hardware_payload.undervoltage_220V_byte;
        m_latestRawData.ov_220v = m_latestRawData.hardware_payload.overvoltage_220V_byte;
        qDebug() << "Type S parsed. UV:" << m_latestRawData.uv_220v << "V, OV:" << m_latestRawData.ov_220v << "V";
        success = true;
        if (!m_handshakeComplete) {
            m_handshakeComplete = true;
            m_retryCount = 0;
            qDebug() << "Nhs_driver: Handshake via ring buffer complete!";
            emit initializationSuccess(); // Report success to the API
            UpsData silentData;
            silentData.state = UpsMonitor::UpsState::Unknown;
            silentData.statusMessage = tr("Connected, waiting for data...");
            emit dataReceived(silentData);
            m_monitorTimer->start(MONITOR_TIMEOUT);
        }
    }
    if (success) {
        m_latestUpsData = convertRawToUpsData(); // Convert to generic format
        if (packet_type == 'D' && m_initialSDataReceived) {
            emit dataReceived(m_latestUpsData);  // Send to GUI/Service
        }
    }
    return success;
}

void Nhs_driver::readData() {
    // const QByteArray newData = m_serialPort->readAll();
    // if (newData.isEmpty()) return;

    // // 1. Fast copy to ring buffer using memcpy
    // int bytesToRead = newData.size();
    // int spaceToEnd = BUFFER_SIZE - m_head;

    // if (bytesToRead <= spaceToEnd) {
    //     std::memcpy(&m_ringBuffer[m_head], newData.constData(), bytesToRead);
    // } else {
    //     std::memcpy(&m_ringBuffer[m_head], newData.constData(), spaceToEnd);
    //     std::memcpy(&m_ringBuffer[0], newData.constData() + spaceToEnd, bytesToRead - spaceToEnd);
    // }
    // m_head = (m_head + bytesToRead) & BUFFER_MASK;

    // // 2. Only start the loop if we have seen a packet end (0xFE)
    // if (!newData.isEmpty() && static_cast<uint8_t>(newData.back()) != 0xFE) {
    //     return;
    // }
//---
    const QByteArray newData = m_serialPort->readAll();
    for (char c : newData) {
        m_ringBuffer[m_head] = static_cast<uint8_t>(c);
        m_head = (m_head + 1) & BUFFER_MASK;
    }
//---
    // const QByteArray newData = m_serialPort->readAll();
    // int bytesToRead = newData.size();
    // int spaceToEnd = BUFFER_SIZE - m_head;

    // if (bytesToRead <= spaceToEnd) {
    //     std::memcpy(&m_ringBuffer[m_head], newData.constData(), bytesToRead);
    // } else {
    //     std::memcpy(&m_ringBuffer[m_head], newData.constData(), spaceToEnd);
    //     std::memcpy(&m_ringBuffer[0], newData.constData() + spaceToEnd, bytesToRead - spaceToEnd);
    // }
    // m_head = (m_head + bytesToRead) & BUFFER_MASK;
//---
    while (((m_head - m_tail) & BUFFER_MASK) >= 9) {
        if (m_ringBuffer[m_tail] != 0xFF) {
            m_tail = (m_tail + 1) & BUFFER_MASK;
            continue;
        }
        int packetLen = m_ringBuffer[(m_tail + 1) & BUFFER_MASK];
        if (packetLen != PACKET_LEN_D && packetLen != PACKET_LEN_S) {
            m_tail = (m_tail + 1) & BUFFER_MASK;
            continue;
        }
        if (((m_head - m_tail) & BUFFER_MASK) < packetLen) break;

        // Validate checksum directly on the ring buffer
        quint8 packet_type = m_ringBuffer[(m_tail + 2) & BUFFER_MASK];
        const int data_len = (packet_type == 'D') ? 18 : 15;
        quint8 expected_cs = m_ringBuffer[(m_tail + packetLen - 2) & BUFFER_MASK];
        quint8 last_byte = m_ringBuffer[(m_tail + packetLen - 1) & BUFFER_MASK];
        if (last_byte == 0xFE && calculate_checksum_ring(m_tail, data_len) == expected_cs) {
            parse_packet_ring(m_tail, packetLen);
        } else {
            // Use qWarning instead of qDebug for real errors, this is more efficient and stands out better.
            qDebug() << "Nhs_driver: Checksum mismatch!";
        }
        m_tail = (m_tail + packetLen) & BUFFER_MASK;
    }
}

void Nhs_driver::onMonitorTimeout() {
    if (!m_serialPort->isOpen()) {
        // The cable is probably still out or the port is gone
        tryOpenPort();
        return;
    }

    if (!m_handshakeComplete) {
        // We are still trying to complete the handshake
        sendInitiatorCommand();
    } else {
        // Normal operation: we have had data before, but now it is silent
        // Perhaps the UPS has failed or the connection is weak.
        qDebug() << "Nhs_driver: No data received (Timeout). Restarting handshake...";
        m_handshakeComplete = false;
        sendInitiatorCommand();
    }
}

void Nhs_driver::sendInitiatorCommand() {
    // If we are already done, we don't need to do this (safety check)
    if (m_handshakeComplete) return;
    if (m_retryCount >= MAX_RETRIES) {
        // FATAL ERROR: We tried 5x and received no S-record.
        QString error = QString(tr("Handshake failed: No S-record received after %1 attempts.")).arg(m_retryCount);
        qDebug() << "Nhs_driver:" << error;

        // Stop everything and report the error to the API layer -> This triggers the 5s recovery timer!
        m_monitorTimer->stop();
        emit initializationFailure(error);
        return;
    }

    m_retryCount++;
    qDebug() << "Nhs_driver: Sending S-command (Attempt" << m_retryCount << "of" << MAX_RETRIES << ")...";

    // Send bytes
    QByteArray command = getCommandS(); // Ensure this function is available
    if (m_serialPort->write(command) == -1) {
        qDebug() << "Write error:" << m_serialPort->errorString();
    }
    m_serialPort->flush();

    // Start the timer for the next attempt.
    // If there is no response within HANDSHAKE_TIMEOUT (1500ms), the timer calls 'onMonitorTimeout'.
    m_monitorTimer->start(HANDSHAKE_TIMEOUT);
}

void Nhs_driver::stopDriver() {
    // This function is now guaranteed to run in the Worker Thread thanks to the BlockingQueuedConnection in cleanupDriver.

    qDebug() << "Nhs_driver: Stop signal received.";
    // This now runs safely in the correct thread thanks to the BlockingQueuedConnection
    if (m_monitorTimer) {
        m_monitorTimer->stop();
        qDebug() << "Nhs_driver: Timer successfully stopped in thread:" << QThread::currentThreadId();
    }

    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
        qDebug() << "Nhs_driver: Serial port closed.";
    }
}

void Nhs_driver::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError) return;
    qDebug() << "Nhs_driver: Serial error detected:" << error << "-" << m_serialPort->errorString();

    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        qDebug() << "Nhs_driver: USB connection physically lost!";

        if (m_serialPort->isOpen()) {
            m_serialPort->close();
        }

        m_handshakeComplete = false;
        m_initialSDataReceived = false;

        // Signal the GUI that the connection is lost
        UpsData errorData;
        errorData.state = UpsMonitor::UpsState::Unknown;
        errorData.statusMessage = tr("USB Connection lost (Recovering...)");
        emit dataReceived(errorData);

        // The m_monitorTimer keeps running and will attempt tryOpenPort() every 3 seconds via onMonitorTimeout().
    }
}

void Nhs_driver::closePort()
{
    if (m_serialPort) {
        if (m_serialPort->isOpen()) {
            m_serialPort->close(); // Close the hardware connection
        }
        delete m_serialPort; // Clean up memory
        m_serialPort = nullptr;
    }
}

bool Nhs_driver::tryOpenPort() {
    if (m_serialPort->isOpen()) return true;

    m_serialPort->setPortName(m_portName);
    if (m_serialPort->open(QIODevice::ReadWrite)) {
        m_serialPort->setDataTerminalReady(true);
        m_serialPort->setRequestToSend(true);
        qDebug() << "Nhs_driver: Port successfully opened:" << m_portName;

        // Start handshake cycle
        QTimer::singleShot(500, this, &Nhs_driver::sendInitiatorCommand);
        return true;
    }
    return false;
}
