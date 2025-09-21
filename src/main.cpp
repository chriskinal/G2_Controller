#include <Arduino.h>
#include "Config.h"
#include "ModbusVFD.h"
#include "WiFiManager.h"

// Global objects
ModbusVFD vfd;
WiFiManager wifiManager;

// Test variables
float testFrequency = 0.0;
float frequencyStep = 10.0;  // Changed to 10Hz steps as requested
bool testRunning = false;     // Track test state independently
unsigned long lastUpdateTime = 0;
unsigned long lastCommandTime = 0;

void setup() {
    // Initialize debug serial
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    while (!DEBUG_SERIAL && millis() < 3000) {
        ; // Wait for serial port to connect
    }

    DEBUG_PRINTLN("\n=== G20 VFD Controller - Milestone 2 Test ===");
    DEBUG_PRINTLN("WiFi Configuration & Modbus Communication");
    DEBUG_PRINTLN("==========================================\n");

    // Initialize WiFi Manager
    DEBUG_PRINTLN("Initializing WiFi Manager...");
    if (wifiManager.begin()) {
        if (wifiManager.isConnected()) {
            DEBUG_PRINTF("✓ Connected to WiFi: %s\n", wifiManager.getSSID().c_str());
            DEBUG_PRINTF("✓ IP Address: %s\n", wifiManager.getIP().c_str());
            DEBUG_PRINTF("✓ Signal Strength: %d dBm\n", wifiManager.getRSSI());
        } else if (wifiManager.isAPMode()) {
            DEBUG_PRINTLN("✓ Started in AP mode");
            DEBUG_PRINTF("✓ AP SSID: %s\n", wifiManager.getAPSSID().c_str());
            DEBUG_PRINTF("✓ AP IP: %s\n", wifiManager.getAPIP().c_str());
            DEBUG_PRINTLN("✓ Connect to the AP and navigate to 10.0.0.1 to configure WiFi");
        }
    } else {
        DEBUG_PRINTLN("✗ Failed to initialize WiFi Manager!");
    }

    // Initialize VFD communication
    vfd.enableDebug(true);

    DEBUG_PRINTLN("\nInitializing Modbus VFD...");
    if (vfd.begin()) {
        DEBUG_PRINTLN("✓ VFD communication established!");
        DEBUG_PRINTF("Initial status: %s\n", vfd.isRunning() ? "Running" : "Stopped");
    } else {
        DEBUG_PRINTLN("✗ Failed to establish VFD communication!");
        DEBUG_PRINTLN("Check wiring and VFD settings:");
        DEBUG_PRINTLN("  - RS485 connections (A/B, GND)");
        DEBUG_PRINTLN("  - VFD slave ID (default: 1)");
        DEBUG_PRINTLN("  - Baud rate (9600, 8N1)");
    }

    // Set VFD parameters
    VFDParams params;
    params.minFrequency = 0.0;
    params.maxFrequency = 60.0;
    params.rampUpTime = 5.0;
    params.rampDownTime = 5.0;
    vfd.setParameters(params);

    DEBUG_PRINTLN("\nTest sequence:");
    DEBUG_PRINTLN("1. Read VFD status every 1 second");
    DEBUG_PRINTLN("2. Every 5 seconds: Update VFD");
    DEBUG_PRINTLN("3. Pattern: Start at 10Hz, increment by 10Hz to 60Hz, then stop");
    DEBUG_PRINTLN("4. Monitor current, voltage, and frequency\n");

    DEBUG_PRINTLN("IMPORTANT: Make sure VFD is set to:");
    DEBUG_PRINTLN("  - Remote/Serial control mode (not Local)");
    DEBUG_PRINTLN("  - Parameter write enabled");
    DEBUG_PRINTLN("  - Check P3.00 = 3 (RS485 control)");
}

void loop() {
    unsigned long currentTime = millis();

    // Handle WiFi events
    wifiManager.handle();

    // Update VFD status every second
    if (currentTime - lastUpdateTime >= 1000) {
        lastUpdateTime = currentTime;

        if (vfd.updateStatus()) {
            // Print status
            DEBUG_PRINTLN("--- VFD Status Update ---");
            DEBUG_PRINTF("Connected: %s\n", vfd.isConnected() ? "Yes" : "No");
            DEBUG_PRINTF("Status: %s %s\n",
                         vfd.isRunning() ? "RUNNING" : "STOPPED",
                         vfd.isFaulted() ? "[FAULT]" : "");
            DEBUG_PRINTF("Frequency: %.2f Hz\n", vfd.getFrequency());
            DEBUG_PRINTF("Current: %.2f A\n", vfd.getCurrent());
            DEBUG_PRINTF("Voltage: %.2f V\n", vfd.getVoltage());
            DEBUG_PRINTF("Status Word: 0x%04X\n", vfd.getStatusWord());
            DEBUG_PRINTLN("------------------------\n");
        } else {
            DEBUG_PRINTLN("✗ Failed to read VFD status!");
        }
    }

    // Send commands every 5 seconds
    if (currentTime - lastCommandTime >= 5000) {
        lastCommandTime = currentTime;

        if (vfd.isConnected()) {
            if (!testRunning) {
                // Start VFD
                DEBUG_PRINTLN(">>> Starting VFD...");
                testFrequency = 10.0;  // Start at 10Hz

                if (vfd.setFrequency(testFrequency)) {
                    DEBUG_PRINTF("✓ Set frequency to %.2f Hz\n", testFrequency);
                }

                if (vfd.start()) {
                    DEBUG_PRINTLN("✓ VFD start command sent");
                    testRunning = true;
                } else {
                    DEBUG_PRINTLN("✗ Failed to start VFD");
                }
            } else {
                // VFD is running - update frequency or stop
                if (testFrequency < 60.0) {  // Changed to 60Hz as requested
                    // Increment frequency
                    testFrequency += frequencyStep;
                    DEBUG_PRINTF(">>> Changing frequency to %.2f Hz...\n", testFrequency);

                    if (vfd.setFrequency(testFrequency)) {
                        DEBUG_PRINTLN("✓ Frequency updated");
                    } else {
                        DEBUG_PRINTLN("✗ Failed to update frequency");
                    }
                } else {
                    // Stop VFD
                    DEBUG_PRINTLN(">>> Stopping VFD...");
                    if (vfd.stop()) {
                        DEBUG_PRINTLN("✓ VFD stop command sent");
                        testFrequency = 0.0;
                        testRunning = false;
                    } else {
                        DEBUG_PRINTLN("✗ Failed to stop VFD");
                    }
                }
            }
        }
    }

    // Small delay to prevent overwhelming the serial output
    delay(10);
}