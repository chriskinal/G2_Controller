#include <Arduino.h>
#include "Config.h"
#include "ModbusVFD.h"
#include "WiFiManager.h"
#include "WebInterface.h"

// Global objects
ModbusVFD vfd;
WiFiManager wifiManager;
WebInterface* webInterface = nullptr;

void setup() {
    // Initialize debug serial
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    while (!DEBUG_SERIAL && millis() < 3000) {
        ; // Wait for serial port to connect
    }

    DEBUG_PRINTLN("\n=== G20 VFD Controller ===");
    DEBUG_PRINTLN("Version: " FIRMWARE_VERSION);
    DEBUG_PRINTLN("==========================\n");

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
    vfd.enableDebug(false);  // Disable debug for cleaner operation

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

    // Initialize Web Interface if WiFi is ready (either connected or AP mode)
    if (wifiManager.isConnected() || wifiManager.isAPMode()) {
        DEBUG_PRINTLN("\nInitializing Web Interface...");
        webInterface = new WebInterface(vfd);
        if (webInterface->begin()) {
            DEBUG_PRINTLN("✓ Web Interface started!");
            DEBUG_PRINTF("✓ WebSocket server on port 81\n");
        } else {
            DEBUG_PRINTLN("✗ Failed to start Web Interface!");
            delete webInterface;
            webInterface = nullptr;
        }
    }

    DEBUG_PRINTLN("\nReady!");
    if (wifiManager.isConnected() && webInterface) {
        DEBUG_PRINTLN("Control interface available at:");
        DEBUG_PRINTF("  http://%s\n", wifiManager.getIP().c_str());
        DEBUG_PRINTF("  http://g20-controller.local\n");
    }
}

void loop() {
    // Handle WiFi events
    wifiManager.handle();

    // Handle web interface if active
    if (webInterface) {
        webInterface->handle();
    }

    // Check if we need to start web interface after WiFi is ready
    if (!webInterface && (wifiManager.isConnected() || wifiManager.isAPMode())) {
        DEBUG_PRINTLN("\nStarting Web Interface...");
        webInterface = new WebInterface(vfd);
        if (webInterface->begin()) {
            DEBUG_PRINTLN("✓ Web Interface started!");
            DEBUG_PRINTF("✓ WebSocket server on port 81\n");
            if (wifiManager.isConnected()) {
                DEBUG_PRINTLN("Control interface available at:");
                DEBUG_PRINTF("  http://%s\n", wifiManager.getIP().c_str());
                DEBUG_PRINTF("  http://g20-controller.local\n");
            } else {
                DEBUG_PRINTLN("Control interface available at:");
                DEBUG_PRINTF("  http://%s (AP mode)\n", wifiManager.getIP().c_str());
            }
        } else {
            DEBUG_PRINTLN("✗ Failed to start Web Interface!");
            delete webInterface;
            webInterface = nullptr;
        }
    }

    // Small delay to prevent CPU hogging
    delay(1);
}