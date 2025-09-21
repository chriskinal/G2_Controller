// Quick utility to clear stored WiFi credentials
// Compile with: build_src_filter = +<*> -<main.cpp>

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

void setup() {
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    delay(2000);

    DEBUG_PRINTLN("\n=== WiFi Credential Clear Utility ===");

    Preferences preferences;
    preferences.begin("wifi", false);

    // Clear all WiFi settings
    preferences.clear();

    DEBUG_PRINTLN("WiFi credentials cleared!");
    DEBUG_PRINTLN("You can now upload the main firmware");
    DEBUG_PRINTLN("===================================\n");
}

void loop() {
    // Nothing to do
    delay(1000);
}