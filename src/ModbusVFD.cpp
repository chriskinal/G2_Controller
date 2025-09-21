#include "ModbusVFD.h"

// Static member initialization
ModbusVFD* ModbusVFD::instance = nullptr;

ModbusVFD::ModbusVFD() :
    connected(false),
    debugEnabled(false),
    slaveId(MODBUS_SLAVE_ID),
    lastCommandTime(0)
{
    instance = this;

    // Initialize status
    memset(&status, 0, sizeof(status));

    // Set default parameters
    parameters.minFrequency = 0.0;
    parameters.maxFrequency = 60.0;
    parameters.rampUpTime = 10.0;
    parameters.rampDownTime = 10.0;
}

ModbusVFD::~ModbusVFD() {
    instance = nullptr;
}

bool ModbusVFD::begin(uint8_t slaveId) {
    this->slaveId = slaveId;

    // Initialize RS485 serial port
    RS485_SERIAL.begin(RS485_BAUD_RATE, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);

    // Initialize direction control pin
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW); // Receive mode by default

    // Initialize Modbus
    modbus.begin(slaveId, RS485_SERIAL);

    // Set callbacks for RS485 direction control
    modbus.preTransmission(preTransmissionCallback);
    modbus.postTransmission(postTransmissionCallback);

    DEBUG_PRINTLN("ModbusVFD: Initialized");
    DEBUG_PRINTF("  Slave ID: %d\n", slaveId);
    DEBUG_PRINTF("  Baud Rate: %d\n", RS485_BAUD_RATE);
    DEBUG_PRINTF("  TX Pin: %d, RX Pin: %d, DE Pin: %d\n",
                 RS485_TX_PIN, RS485_RX_PIN, RS485_DE_PIN);

    // Try to read status to check connection
    delay(100);
    connected = updateStatus();

    return connected;
}

bool ModbusVFD::setFrequency(float frequencyHz) {
    if (!connected) return false;

    // Constrain frequency to limits
    frequencyHz = constrain(frequencyHz, parameters.minFrequency, parameters.maxFrequency);

    // Convert Hz to internal format (typically Hz * 100)
    uint16_t freqValue = (uint16_t)(frequencyHz * 100);

    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: Setting frequency to %.2f Hz (0x%04X)\n",
                     frequencyHz, freqValue);
    }

    return writeRegister(REG_FREQUENCY_WRITE, freqValue);
}

bool ModbusVFD::start(bool reverse) {
    if (!connected) return false;

    uint16_t command = reverse ? CMD_RUN_REV : CMD_RUN_FWD;

    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: Starting VFD %s\n", reverse ? "reverse" : "forward");
    }

    return sendCommand(command);
}

bool ModbusVFD::stop() {
    if (!connected) return false;

    if (debugEnabled) {
        DEBUG_PRINTLN("ModbusVFD: Stopping VFD");
    }

    return sendCommand(CMD_STOP);
}

bool ModbusVFD::reset() {
    if (!connected) return false;

    if (debugEnabled) {
        DEBUG_PRINTLN("ModbusVFD: Resetting VFD");
    }

    return sendCommand(CMD_RESET);
}

bool ModbusVFD::jog(bool reverse) {
    if (!connected) return false;

    uint16_t command = reverse ? CMD_JOG_REV : CMD_JOG_FWD;

    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: Jogging VFD %s\n", reverse ? "reverse" : "forward");
    }

    return sendCommand(command);
}

bool ModbusVFD::updateStatus() {
    uint16_t buffer[4];
    bool readSuccess = false;

    // Read actual status register (0x2101) - contains run/stop/direction
    if (!readRegisters(REG_STATUS_READ, 1, &buffer[0])) {
        if (debugEnabled) {
            DEBUG_PRINTLN("ModbusVFD: Failed to read status register");
        }
        connected = false;
        return false;
    }

    // Status read successful
    readSuccess = true;
    connected = true;
    status.lastUpdateTime = millis();

    // Parse status word
    status.statusWord = buffer[0];
    parseStatusWord(status.statusWord);

    // Also read error/warning status at 0x2100
    if (debugEnabled) {
        uint16_t errorStatus = 0;
        if (readRegisters(REG_ERROR_STATUS, 1, &errorStatus)) {
            DEBUG_PRINTF("ModbusVFD: Error/Warning status (0x2100) = 0x%04X\n", errorStatus);
            if (errorStatus != 0) {
                DEBUG_PRINTF("  High byte (Warning): 0x%02X, Low byte (Error): 0x%02X\n",
                             (errorStatus >> 8) & 0xFF, errorStatus & 0xFF);
            }
        }
    }

    // Read additional registers based on manual
    // Note: We need to read them separately as they're not consecutive
    // RTU timing is handled in pre/postTransmission
    uint16_t freqOut = 0, current = 0, voltage = 0;

    // Try to read output frequency (0x2103)
    if (debugEnabled) {
        DEBUG_PRINTF("Attempting to read frequency at 0x%04X\n", REG_FREQ_OUT_READ);
    }
    if (readRegisters(REG_FREQ_OUT_READ, 1, &freqOut)) {
        status.actualFrequency = freqOut / 100.0;  // XXX.XX Hz format
        if (debugEnabled) {
            DEBUG_PRINTF("  Read frequency: %d (%.2f Hz)\n", freqOut, status.actualFrequency);
        }
    } else {
        // Try with offset -1 (some implementations need this)
        if (debugEnabled) {
            DEBUG_PRINTF("  Trying address 0x%04X\n", REG_FREQ_OUT_READ - 1);
        }
        if (readRegisters(REG_FREQ_OUT_READ - 1, 1, &freqOut)) {
            status.actualFrequency = freqOut / 100.0;
            if (debugEnabled) {
                DEBUG_PRINTF("  Success with -1 offset: %d (%.2f Hz)\n", freqOut, status.actualFrequency);
            }
        } else {
            if (debugEnabled) {
                DEBUG_PRINTLN("  Failed to read frequency");
            }
        }
    }

    // Try to read output current (0x2104)
    if (debugEnabled) {
        DEBUG_PRINTF("Attempting to read current at 0x%04X\n", REG_CURRENT_READ);
    }
    if (readRegisters(REG_CURRENT_READ, 1, &current)) {
        status.outputCurrent = current / 100.0;  // XX.XX A format
        if (debugEnabled) {
            DEBUG_PRINTF("  Read current: %d (%.2f A)\n", current, status.outputCurrent);
        }
    } else {
        if (debugEnabled) {
            DEBUG_PRINTLN("  Failed to read current");
        }
    }

    // Try to read output voltage (0x2106)
    if (debugEnabled) {
        DEBUG_PRINTF("Attempting to read voltage at 0x%04X\n", REG_VOLTAGE_READ);
    }
    if (readRegisters(REG_VOLTAGE_READ, 1, &voltage)) {
        status.outputVoltage = voltage / 10.0;  // XXX.X V format
        if (debugEnabled) {
            DEBUG_PRINTF("  Read voltage: %d (%.1f V)\n", voltage, status.outputVoltage);
        }
    } else {
        if (debugEnabled) {
            DEBUG_PRINTLN("  Failed to read voltage");
        }
    }

    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: Status updated\n");
        DEBUG_PRINTF("  Status: 0x%04X %s\n", status.statusWord,
                     status.isRunning ? "Running" : "Stopped");
        DEBUG_PRINTF("  Frequency: %.2f Hz\n", status.actualFrequency);
        DEBUG_PRINTF("  Current: %.2f A\n", status.outputCurrent);
        DEBUG_PRINTF("  Voltage: %.2f V\n", status.outputVoltage);
    }

    return true;
}

float ModbusVFD::getFrequency() {
    return status.actualFrequency;
}

float ModbusVFD::getCurrent() {
    return status.outputCurrent;
}

float ModbusVFD::getVoltage() {
    return status.outputVoltage;
}

uint16_t ModbusVFD::getStatusWord() {
    return status.statusWord;
}

bool ModbusVFD::setParameters(const VFDParams& params) {
    parameters = params;
    return true;
}

// Private helper functions

void ModbusVFD::preTransmission() {
    // RTU mode requires >10ms silent interval before transmission
    delay(15);  // 15ms to be safe
    digitalWrite(RS485_DE_PIN, HIGH);  // Enable transmit mode
    delayMicroseconds(100);  // Small delay for direction change
}

void ModbusVFD::postTransmission() {
    // Wait for last byte to transmit (at 9600 baud, ~1ms per byte)
    delay(2);
    digitalWrite(RS485_DE_PIN, LOW);   // Enable receive mode
    // RTU mode requires >10ms silent interval after transmission
    delay(15);  // 15ms to be safe
}

bool ModbusVFD::sendCommand(uint16_t command) {
    return writeRegister(REG_CONTROL_WRITE, command);
}

bool ModbusVFD::writeRegister(uint16_t address, uint16_t value) {
    uint8_t result;

    // Try primary address first
    result = modbus.writeSingleRegister(address, value);
    if (result == modbus.ku8MBSuccess) {
        lastCommandTime = millis();
        if (debugEnabled) {
            DEBUG_PRINTF("ModbusVFD: Write success at 0x%04X\n", address);
        }
        return true;
    }

    // Try alternative address if this is a control or frequency write
    uint16_t altAddress = address;
    if (address == REG_CONTROL_WRITE) {
        altAddress = REG_CONTROL_WRITE_ALT;
    } else if (address == REG_FREQUENCY_WRITE) {
        altAddress = REG_FREQUENCY_WRITE_ALT;
    }

    if (altAddress != address) {
        if (debugEnabled) {
            DEBUG_PRINTF("ModbusVFD: Trying alternative address 0x%04X\n", altAddress);
        }
        result = modbus.writeSingleRegister(altAddress, value);
        if (result == modbus.ku8MBSuccess) {
            lastCommandTime = millis();
            return true;
        }
    }

    // Try multiple register write as last resort
    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: Trying multiple register write\n");
    }
    modbus.setTransmitBuffer(0, value);
    result = modbus.writeMultipleRegisters(altAddress, 1);

    if (result == modbus.ku8MBSuccess) {
        lastCommandTime = millis();
        return true;
    }

    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: Write failed at addresses 0x%04X and 0x%04X, error: 0x%02X\n",
                     address, altAddress, result);
        DEBUG_PRINTF("  Error codes: 0x01=Illegal Function, 0x02=Illegal Address,\n");
        DEBUG_PRINTF("  0x03=Illegal Value, 0x04=Slave Failure\n");
    }

    return false;
}

bool ModbusVFD::readRegisters(uint16_t address, uint16_t count, uint16_t* buffer) {
    // Debug: Show what we're trying to read
    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: Reading %d registers from 0x%04X\n", count, address);
    }

    // Try holding registers first (Function 03)
    uint8_t result = modbus.readHoldingRegisters(address, count);

    if (result == modbus.ku8MBSuccess) {
        for (uint16_t i = 0; i < count; i++) {
            buffer[i] = modbus.getResponseBuffer(i);
        }
        return true;
    }

    // If holding registers fail, try input registers (Function 04)
    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: FC03 failed (0x%02X), trying FC04 for address 0x%04X\n", result, address);
    }

    result = modbus.readInputRegisters(address, count);

    if (result == modbus.ku8MBSuccess) {
        for (uint16_t i = 0; i < count; i++) {
            buffer[i] = modbus.getResponseBuffer(i);
        }
        return true;
    }

    if (debugEnabled) {
        DEBUG_PRINTF("ModbusVFD: Read registers 0x%04X failed, error: 0x%02X\n",
                     address, result);
        DEBUG_PRINTF("  0xE0=Timeout, 0xE2=Timeout, 0x02=Illegal Address\n");
    }

    return false;
}

void ModbusVFD::parseStatusWord(uint16_t statusWord) {
    // Parse status bits from register 0x2101 according to manual
    // Bits 1-0: Drive status (00=Stop, 01=Decelerating, 10=Standby, 11=Operating)
    uint8_t driveStatus = statusWord & 0x03;
    status.isRunning = (driveStatus == 0x03);  // 11B = Operating

    // Bits 4-3: Operation direction
    // 00B: FWD stop
    // 01B: From REV running to FWD running
    // 10B: From FWD running to REV running
    // 11B: REV running
    uint8_t direction = (statusWord >> 3) & 0x03;
    bool isReverse = (direction == 0x03);  // 11B = REV running

    // Bit 2: JOG command
    bool isJogging = (statusWord & 0x04) != 0;

    // Bit 8: Master frequency controlled by communication
    bool freqByComm = (statusWord & 0x0100) != 0;

    // Bit 9: Master frequency controlled by analog/external
    bool freqByAnalog = (statusWord & 0x0200) != 0;

    // Bit 10: Operation command controlled by communication
    bool cmdByComm = (statusWord & 0x0400) != 0;

    // Bit 11: Parameter locked
    bool paramLocked = (statusWord & 0x0800) != 0;

    // Bit 12: Enable to copy parameters from keypad
    bool copyEnabled = (statusWord & 0x1000) != 0;

    // Set derived status
    status.isReady = (driveStatus == 0x02);  // 10B = Standby
    status.isFaulted = false;  // Fault info is in register 0x2100

    if (debugEnabled) {
        DEBUG_PRINTF("  Status Word Details: 0x%04X\n", statusWord);
        DEBUG_PRINTF("  Raw bits: ");
        for (int i = 15; i >= 0; i--) {
            DEBUG_PRINTF("%d", (statusWord >> i) & 1);
            if (i % 4 == 0) DEBUG_PRINTF(" ");
        }
        DEBUG_PRINTLN();
        DEBUG_PRINTF("  Bits 1-0 (Drive): %d%d, Bits 4-3 (Dir): %d%d\n",
                     (statusWord >> 1) & 1, statusWord & 1,
                     (statusWord >> 4) & 1, (statusWord >> 3) & 1);
        DEBUG_PRINTF("  Drive Status: %s, Direction: %s%s\n",
                     driveStatus == 0 ? "Stop" :
                     driveStatus == 1 ? "Decelerating" :
                     driveStatus == 2 ? "Standby" : "Operating",
                     direction == 0 ? "FWD Stop" :
                     direction == 1 ? "REV→FWD" :
                     direction == 2 ? "FWD→REV" : "REV Running",
                     isJogging ? " (JOG)" : "");
        DEBUG_PRINTF("  Control: Freq=%s, Cmd=%s, Param=%s\n",
                     freqByComm ? "Comm" : "Terminal",
                     cmdByComm ? "Comm" : "Terminal",
                     paramLocked ? "Locked" : "Unlocked");
    }
}

// Static callback implementations

void ModbusVFD::preTransmissionCallback() {
    if (instance) {
        instance->preTransmission();
    }
}

void ModbusVFD::postTransmissionCallback() {
    if (instance) {
        instance->postTransmission();
    }
}