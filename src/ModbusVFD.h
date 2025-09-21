#ifndef MODBUS_VFD_H
#define MODBUS_VFD_H

#include <Arduino.h>
#include <ModbusMaster.h>
#include "Config.h"

// VFD Status structure
struct VFDStatus {
    uint16_t statusWord;
    float actualFrequency;
    float outputCurrent;
    float outputVoltage;
    bool isRunning;
    bool isFaulted;
    bool isReady;
    uint32_t lastUpdateTime;
};

// VFD Parameters structure
struct VFDParams {
    float minFrequency;
    float maxFrequency;
    float rampUpTime;
    float rampDownTime;
};

class ModbusVFD {
public:
    ModbusVFD();
    ~ModbusVFD();

    // Initialize the Modbus communication
    bool begin(uint8_t slaveId = MODBUS_SLAVE_ID);

    // Control functions
    bool setFrequency(float frequencyHz);
    bool start(bool reverse = false);
    bool stop();
    bool reset();
    bool jog(bool reverse = false);

    // Read functions
    bool updateStatus();
    float getFrequency();
    float getCurrent();
    float getVoltage();
    uint16_t getStatusWord();

    // Status check functions
    bool isRunning() const { return status.isRunning; }
    bool isFaulted() const { return status.isFaulted; }
    bool isReady() const { return status.isReady; }
    bool isConnected() const { return connected; }

    // Get full status
    const VFDStatus& getStatus() const { return status; }

    // Parameter functions
    bool setParameters(const VFDParams& params);
    const VFDParams& getParameters() const { return parameters; }

    // Debug functions
    void enableDebug(bool enable) { debugEnabled = enable; }

private:
    ModbusMaster modbus;
    VFDStatus status;
    VFDParams parameters;

    bool connected;
    bool debugEnabled;
    uint8_t slaveId;
    uint32_t lastCommandTime;

    // Helper functions
    void preTransmission();
    void postTransmission();
    bool sendCommand(uint16_t command);
    bool writeRegister(uint16_t address, uint16_t value);
    bool readRegisters(uint16_t address, uint16_t count, uint16_t* buffer);
    void parseStatusWord(uint16_t statusWord);

    // Static callback functions for ModbusMaster
    static void preTransmissionCallback();
    static void postTransmissionCallback();

    // Instance pointer for callbacks
    static ModbusVFD* instance;
};

#endif // MODBUS_VFD_H