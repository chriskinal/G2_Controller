#include <Arduino.h>

unsigned long lastPrint = 0;
int counter = 0;

// Use built-in LED or GPIO 48 for ESP32-S3
#define LED_PIN 48

void setup() {
    // Configure LED
    pinMode(LED_PIN, OUTPUT);

    // Flash LED 3 times to show we're starting
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }

    Serial.begin(115200);

    // Don't wait for serial, just start
    delay(2000);  // Give time for serial monitor to connect

    Serial.println("\n=== Serial Test Program ===");
    Serial.println("You should see a message every second");
    Serial.println("LED should blink with each message");
    Serial.println("==========================\n");
}

void loop() {
    if (millis() - lastPrint >= 1000) {
        lastPrint = millis();
        counter++;

        // Toggle LED
        digitalWrite(LED_PIN, counter % 2);

        Serial.print("Test message #");
        Serial.print(counter);
        Serial.print(" at ");
        Serial.print(millis());
        Serial.println(" ms");
        Serial.flush();  // Force output
    }

    delay(10);
}