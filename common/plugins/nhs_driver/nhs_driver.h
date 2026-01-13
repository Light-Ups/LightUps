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

#ifndef NHS_DRIVER_H
#define NHS_DRIVER_H

#include <QtSerialPort/QSerialPort>
#include <QByteArray>
#include <QString>
#include <QTimer>
#include <QDebug>
#include "i_ups_driver.h"

// Ensure the compiler packs the structs (required for the NHS protocol)
// The empty struct resolves a compiler bug/quirk with the pragma stack.
typedef struct {} dummy_struct_for_compiler_quirk;

// nhs_driver.h or a separate header file (ups_status_bits.h)
namespace NhsStatusBits {
// Bits in the statusval (byte)
const quint8 FREQUENCY_ASYNC       = 0x01;  // Bit 0
const quint8 BATTERY_LOW_CRITICAL  = 0x02;  // Bit 1
// const quint8 BIT_2_UNKNOWN         = 0x04;  // Bit 2
const quint8 INVERTER_ACTIVE       = 0x08;  // Bit 3
const quint8 BATTERY_CHARGING      = 0x10;  // Bit 4
// const quint8 BIT_5_UNKNOWN         = 0x20;  // Bit 5
// const quint8 BIT_6_UNKNOWN         = 0x40;  // Bit 6
const quint8 BATTERY_FLOW_ACTIVE   = 0x80;  // Bit 7
}

#pragma pack(push, 1)

// The 16 bytes of the Realtime Status data-payload (Type 'D')

    typedef struct {
    quint8 vacinrms_low;
    quint8 vacinrms_high;
    quint8 vdcmed_low;
    quint8 vdcmed_high;
    quint8 potrms;
    quint8 vacinrmsmin_low;
    quint8 vacinrmsmin_high;
    quint8 vacinrmsmax_low;
    quint8 vacinrmsmax_high;
    quint8 vacoutrms_low;
    quint8 vacoutrms_high;
    quint8 tempmed_low;
    quint8 tempmed_high;
    quint8 icarregrms;
    quint8 statusval;
    quint8 unknown_status;
} nhs_data_payload_t;

// The 14 bytes of the core hardware data-payload (Type 'S')
typedef struct {
    quint8 unknown_id_byte_1;
    quint8 unknown_id_byte_2;
    quint8 unknown_id_byte_3;
    quint8 unknown_id_byte_4;
    quint8 unknown_id_byte_5;

    quint8 undervoltage_127V_byte;
    quint8 overvoltage_127V_byte;
    quint8 undervoltage_220V_byte;
    quint8 overvoltage_220V_byte;

    quint8 output_voltage_byte;
    quint8 input_voltage_byte;
    quint8 unknown_byte_6;
    quint8 unknown_byte_7;
} nhs_hardware_payload_t;

// Global structure to store the raw parsed data
typedef struct {
    // Raw payloads
    nhs_data_payload_t payload;
    nhs_hardware_payload_t hardware_payload;

    // Parsed and converted values
    quint16 input_voltage_v;
    float battery_voltage_v;
    quint16 output_voltage_v;
    quint16 temperature_c;
    quint8 power_rms_percent;
    quint16 input_voltage_min_v;
    quint16 input_voltage_max_v;

    // --- New: All 8 status flags ---
    bool s_battery_mode;            // Bit 0 (already present)
    bool s_battery_low;             // Bit 1 (NEW)
    bool s_network_failure;         // Bit 2 (already present)
    bool s_fast_network_failure;    // Bit 3 (NEW)
    bool s_220_in;                  // Bit 4 (NEW)
    bool s_220_out;                 // Bit 5 (NEW)
    bool s_bypass_on;               // Bit 6 (NEW)
    bool s_charger_on;              // Bit 7 (NEW)

    quint8 uv_220v;
    quint8 ov_220v;

} pkt_data_t;
#pragma pack(pop)

class  Nhs_driver: public IUpsDriver
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IUpsDriver_iid FILE "nhs_driver.json")
    Q_INTERFACES(IUpsDriver)

public:
    Nhs_driver(QObject *parent = nullptr);
    ~Nhs_driver(); // <--- NEW: Explicit destructor
    bool initialize(const QString& connectionInfo) override;
    QString driverName() const override;

public Q_SLOTS: // <--- NEW: Slot to stop the driver cleanly from outside
    void stopDriver();

private Q_SLOTS:
    // Slots for asynchronous communication
    void readData();               // Triggered by QSerialPort::readyRead
    void onMonitorTimeout();       // <<< NEW: Slot for data loss monitoring/restart
    void handleSerialError(QSerialPort::SerialPortError error);

private:
    QString m_portName;
    QSerialPort *m_serialPort = nullptr;
    QTimer *m_monitorTimer = nullptr;
    pkt_data_t m_latestRawData;
    UpsData m_latestUpsData;               // The data returned by fetchData()
    bool m_initialSDataReceived = false;   // Has the stream already provided D-data?
    void sendInitiatorCommand();

    // Helper Functions (converted from main.c)
    quint8 calculate_checksum_ring(int tail_index, int length);
    bool parse_packet_ring(int tail_index, int packetLen);
    UpsData convertRawToUpsData();
    int calculateBatteryLevelFromVoltage(double voltage) const;

    // New variables for the handshake logic
    int m_retryCount = 0;             // How many times have we tried the S-command?
    bool m_handshakeComplete = false; // Have we received the mandatory S-record yet?

    // Constanten
    const int MAX_RETRIES = 5;          // Maximum 5 attempts (5x 1.5s = 7.5s total))
    const int HANDSHAKE_TIMEOUT = 1500; // 1.5 seconds waiting for response
    const int MONITOR_TIMEOUT = 3000;   // Normal timeout during operation

    // Ring buffer configuration
    static const int BUFFER_SIZE = 128;             // Must be a power of 2
    static const int BUFFER_MASK = BUFFER_SIZE - 1; // Used for the fast & operation
    uint8_t m_ringBuffer[BUFFER_SIZE];              // The actual buffer
    int m_head = 0;                                 // Write index
    int m_tail = 0;                                 // Read index

    void closePort(); // New method
    bool tryOpenPort();
};

#endif // NHS_DRIVER_H
