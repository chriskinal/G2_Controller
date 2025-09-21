#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "Config.h"
#include "SimpleHTTPServer.h"

// WiFi states
enum class WiFiState {
    DISCONNECTED,
    AP_MODE,
    CONNECTING,
    CONNECTED,
    CONNECTION_FAILED
};

// WiFi credentials structure
struct WiFiCredentials {
    char ssid[32];
    char password[64];
    bool valid;
};

class WiFiManager {
public:
    WiFiManager();
    ~WiFiManager();

    // Initialize WiFi manager
    bool begin();

    // Start AP mode for configuration
    bool startAP();

    // Connect to WiFi using stored credentials
    bool connect();

    // Connect to WiFi with new credentials
    bool connect(const String& ssid, const String& password);

    // Save credentials to persistent storage
    bool saveCredentials(const String& ssid, const String& password);

    // Clear stored credentials
    void clearCredentials();

    // Get current state
    WiFiState getState() const { return state; }
    bool isConnected() const { return state == WiFiState::CONNECTED; }
    bool isAPMode() const { return state == WiFiState::AP_MODE; }

    // Get connection info
    String getSSID() const;
    String getIP() const;
    int getRSSI() const;

    // Get AP info
    String getAPIP() const { return "10.0.0.1"; }
    String getAPSSID() const { return AP_SSID; }

    // Handle WiFi events (call from loop)
    void handle();

    // Check if credentials are stored
    bool hasCredentials() const;

private:
    WiFiState state;
    WiFiCredentials credentials;
    Preferences preferences;

    unsigned long connectStartTime;
    unsigned long lastReconnectAttempt;
    int reconnectAttempts;

    // Internal methods
    bool loadCredentials();
    void setState(WiFiState newState);
    void handleConnection();
    void handleAPMode();

    // Static event handlers
    static void onWiFiEvent(WiFiEvent_t event);
    static WiFiManager* instance;

    // DNS server for captive portal
    WiFiUDP dnsUdp;
    bool dnsServerRunning;
    void startDNSServer();
    void stopDNSServer();
    void handleDNS();

    // Web server for configuration
    SimpleHTTPServer* webServer;
    void setupWebRoutes();
    void handleWiFiScan(WiFiClient& client, const String& method, const String& query);
    void handleWiFiConnect(WiFiClient& client, const String& method, const String& query);
    void handleWiFiStatus(WiFiClient& client, const String& method, const String& query);
};

#endif // WIFI_MANAGER_H