#ifndef CONFIG_H
#define CONFIG_H

// Hardware Pin Definitions
#define RS485_TX_PIN    17    // RS485 TX pin
#define RS485_RX_PIN    18    // RS485 RX pin
#define RS485_DE_PIN    21    // RS485 Direction Enable pin

// RS485 Communication Settings
#define RS485_SERIAL    Serial1
#define RS485_BAUD_RATE 9600
#define RS485_CONFIG    SERIAL_8N1

// Modbus Settings
#define MODBUS_SLAVE_ID 1     // G20 VFD default slave ID
#define MODBUS_TIMEOUT  100   // Timeout in milliseconds (reduced for faster response)
#define MODBUS_READ_OFFSET 0  // Some G20 models need -1 offset for read addresses
#define MODBUS_RTU_SILENCE 15 // Silent interval for RTU mode (>10ms required)

// G20 VFD Modbus Register Addresses (from G20_AppC_IO_Parm_Maps.pdf)
// Common G20 addressing: subtract 1 from documentation addresses
// Documentation shows 40001-40002 for write, 30001-30004 for read
// But Modbus uses 0-based addressing

// Write registers - try both common patterns
#define REG_CONTROL_WRITE       0x2000  // Control command (some use 0x2000/8192)
#define REG_FREQUENCY_WRITE     0x2001  // Frequency command (some use 0x2001/8193)

// Alternative write addresses (if above don't work)
#define REG_CONTROL_WRITE_ALT   0x0000  // Alternative: 0-based from 40001
#define REG_FREQUENCY_WRITE_ALT 0x0001  // Alternative: 0-based from 40002

// Read registers - G20 uses 0x21xx range per manual page 5-7
#define REG_ERROR_STATUS        0x2100  // High/Low byte Warning/Error codes
#define REG_STATUS_READ         0x2101  // Drive operation status (run/stop/direction)
#define REG_FREQ_CMD_READ       0x2102  // Frequency command (XXX.XX Hz)
#define REG_FREQ_OUT_READ       0x2103  // Output frequency (XXX.XX Hz)
#define REG_CURRENT_READ        0x2104  // Output current (XX.XX A)
#define REG_DC_BUS_READ         0x2105  // DC bus voltage (XXX.X V)
#define REG_VOLTAGE_READ        0x2106  // Output voltage (XXX.X V)
#define REG_MULTI_SPEED_READ    0x2107  // Current step for multi-step speed
#define REG_COUNTER_READ        0x2109  // Counter value
#define REG_POWER_FACTOR_READ   0x210A  // Output power factor angle
#define REG_TORQUE_READ         0x2113  // Output torque (XXX.X %)
#define REG_MOTOR_SPEED_READ    0x2114  // Actual motor speed (XXXXX rpm)

// Alternative read addresses (for compatibility)
#define REG_STATUS_READ_ALT     0x2100  // Same as primary
#define REG_FREQUENCY_READ_ALT  0x2101  // Same as primary

// G20 Control Commands per manual
// Bits 1-0: 00=No function, 01=Stop, 10=Run, 11=JOG+RUN
// Bits 5-4: 00=No function, 01=FWD, 10=REV, 11=Change direction
#define CMD_STOP        0x0001  // 0000 0001: Stop
#define CMD_RUN_FWD     0x0012  // 0001 0010: Run + FWD direction
#define CMD_RUN_REV     0x0022  // 0010 0010: Run + REV direction
#define CMD_JOG_FWD     0x0013  // 0001 0011: JOG+Run + FWD
#define CMD_JOG_REV     0x0023  // 0010 0011: JOG+Run + REV
#define CMD_RESET       0x0000  // 0000 0000: No function/reset

// Additional control bits for 0x2002
// Bit 0: E.F. (External Fault) ON
// Bit 1: Reset command
// Bit 2: E.B. ON
// Bit 5: Enable fire mode

// WiFi Configuration
#define AP_SSID         "G20_Controller_Setup"
#define AP_PASSWORD     ""
#define AP_CHANNEL      6
#define AP_MAX_CONN     4

// mDNS hostname (access device at g20-controller.local)
#define MDNS_HOSTNAME   "g20-controller"

// Web Server Settings
#define WEB_SERVER_PORT 80
#define WS_PORT         81

// Version Information
#define FIRMWARE_VERSION "0.1.0"
#define HARDWARE_VERSION "ESP32-S3"

// Debug Settings
#define DEBUG_SERIAL    Serial
#define DEBUG_BAUD      115200
#define DEBUG_ENABLED   true

#define DEBUG_PRINT(x)    if(DEBUG_ENABLED) { DEBUG_SERIAL.print(x); }
#define DEBUG_PRINTLN(x)  if(DEBUG_ENABLED) { DEBUG_SERIAL.println(x); }
#define DEBUG_PRINTF(...) if(DEBUG_ENABLED) { DEBUG_SERIAL.printf(__VA_ARGS__); }

#endif // CONFIG_H