#include "WiFiManager.h"
#include <ArduinoJson.h>

// Static member initialization
WiFiManager* WiFiManager::instance = nullptr;

WiFiManager::WiFiManager() :
    state(WiFiState::DISCONNECTED),
    connectStartTime(0),
    lastReconnectAttempt(0),
    reconnectAttempts(0),
    dnsServerRunning(false),
    webServer(nullptr)
{
    instance = this;
    memset(&credentials, 0, sizeof(credentials));
}

WiFiManager::~WiFiManager() {
    stopDNSServer();
    if (webServer) {
        webServer->stop();
        delete webServer;
    }
    instance = nullptr;
}

bool WiFiManager::begin() {
    // Initialize preferences for credential storage
    preferences.begin("wifi", false);

    // Register WiFi event handler
    WiFi.onEvent(onWiFiEvent);

    // Set WiFi mode
    WiFi.mode(WIFI_MODE_NULL);

    // Load stored credentials
    if (loadCredentials()) {
        DEBUG_PRINTLN("WiFiManager: Found stored credentials");
        // Try to connect with stored credentials
        return connect();
    } else {
        DEBUG_PRINTLN("WiFiManager: No stored credentials, starting AP mode");
        // No credentials, start AP mode
        return startAP();
    }
}

bool WiFiManager::startAP() {
    DEBUG_PRINTLN("WiFiManager: Starting AP mode");

    // Disconnect from any network
    WiFi.disconnect(true);
    delay(100);

    // Set AP mode
    WiFi.mode(WIFI_AP);

    // Configure AP (open network if password is empty)
    bool apStarted = false;
    if (strlen(AP_PASSWORD) == 0) {
        // Open AP (no password)
        apStarted = WiFi.softAP(AP_SSID, nullptr, AP_CHANNEL, 0, AP_MAX_CONN);
    } else {
        // Password-protected AP
        apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);
    }

    if (!apStarted) {
        DEBUG_PRINTLN("WiFiManager: Failed to start AP");
        return false;
    }

    // Configure AP IP
    IPAddress ip(10, 0, 0, 1);
    IPAddress gateway(10, 0, 0, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(ip, gateway, subnet);

    setState(WiFiState::AP_MODE);

    DEBUG_PRINTF("WiFiManager: AP started - SSID: %s, IP: %s\n",
                 AP_SSID, WiFi.softAPIP().toString().c_str());

    // Start DNS server for captive portal
    startDNSServer();

    // Start web server
    if (!webServer) {
        webServer = new SimpleHTTPServer();
        setupWebRoutes();
        webServer->begin(80);
    }

    return true;
}

bool WiFiManager::connect() {
    if (!credentials.valid) {
        DEBUG_PRINTLN("WiFiManager: No valid credentials to connect");
        return false;
    }

    return connect(String(credentials.ssid), String(credentials.password));
}

bool WiFiManager::connect(const String& ssid, const String& password) {
    DEBUG_PRINTF("WiFiManager: Connecting to %s\n", ssid.c_str());

    // Stop DNS server if running
    stopDNSServer();

    // Stop web server if running
    if (webServer) {
        webServer->stop();
        delete webServer;
        webServer = nullptr;
    }

    // Disconnect from any network/AP
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    delay(100);

    // Set STA mode
    WiFi.mode(WIFI_STA);

    // Start connection
    WiFi.begin(ssid.c_str(), password.c_str());

    setState(WiFiState::CONNECTING);
    connectStartTime = millis();
    reconnectAttempts = 0;

    return true;
}

bool WiFiManager::saveCredentials(const String& ssid, const String& password) {
    // Store in structure
    strlcpy(credentials.ssid, ssid.c_str(), sizeof(credentials.ssid));
    strlcpy(credentials.password, password.c_str(), sizeof(credentials.password));
    credentials.valid = true;

    // Save to preferences
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putBool("valid", true);

    DEBUG_PRINTF("WiFiManager: Credentials saved for %s\n", ssid.c_str());

    return true;
}

void WiFiManager::clearCredentials() {
    // Clear structure
    memset(&credentials, 0, sizeof(credentials));

    // Clear preferences
    preferences.remove("ssid");
    preferences.remove("password");
    preferences.remove("valid");

    DEBUG_PRINTLN("WiFiManager: Credentials cleared");
}

String WiFiManager::getSSID() const {
    if (state == WiFiState::CONNECTED) {
        return WiFi.SSID();
    }
    return "";
}

String WiFiManager::getIP() const {
    if (state == WiFiState::CONNECTED) {
        return WiFi.localIP().toString();
    } else if (state == WiFiState::AP_MODE) {
        return WiFi.softAPIP().toString();
    }
    return "";
}

int WiFiManager::getRSSI() const {
    if (state == WiFiState::CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

void WiFiManager::handle() {
    switch (state) {
        case WiFiState::CONNECTING:
            handleConnection();
            break;

        case WiFiState::AP_MODE:
            handleAPMode();
            handleDNS();  // Handle DNS requests for captive portal
            if (webServer) {
                webServer->handleClient();  // Handle HTTP requests
            }
            break;

        case WiFiState::CONNECTED:
            // Handle web server requests
            if (webServer) {
                webServer->handleClient();
            }
            break;

        case WiFiState::CONNECTION_FAILED:
            // Retry connection after delay
            if (millis() - lastReconnectAttempt > 30000) {  // 30 seconds
                lastReconnectAttempt = millis();
                if (reconnectAttempts < 3) {
                    DEBUG_PRINTLN("WiFiManager: Retrying connection");
                    connect();
                } else {
                    DEBUG_PRINTLN("WiFiManager: Max reconnect attempts reached, starting AP");
                    DEBUG_PRINTLN("=====================================");
                    DEBUG_PRINTLN("Connect to AP to reconfigure WiFi");
                    DEBUG_PRINTLN("=====================================");
                    startAP();
                }
            }
            break;

        default:
            break;
    }
}

bool WiFiManager::hasCredentials() const {
    return credentials.valid;
}

bool WiFiManager::loadCredentials() {
    // Load from preferences
    String ssid = preferences.getString("ssid", "");
    String password = preferences.getString("password", "");
    bool valid = preferences.getBool("valid", false);

    if (valid && ssid.length() > 0) {
        strlcpy(credentials.ssid, ssid.c_str(), sizeof(credentials.ssid));
        strlcpy(credentials.password, password.c_str(), sizeof(credentials.password));
        credentials.valid = true;
        return true;
    }

    return false;
}

void WiFiManager::setState(WiFiState newState) {
    if (state != newState) {
        state = newState;
        DEBUG_PRINTF("WiFiManager: State changed to %d\n", (int)state);
    }
}

void WiFiManager::handleConnection() {
    // For delayed connection from web interface
    if (connectStartTime > millis()) {
        // Still waiting for delayed start
        return;
    }

    // Check for timeout
    if (millis() - connectStartTime > 20000) {  // 20 second timeout
        DEBUG_PRINTLN("WiFiManager: Connection timeout");
        WiFi.disconnect();
        setState(WiFiState::CONNECTION_FAILED);
        reconnectAttempts++;
    }
}

void WiFiManager::handleAPMode() {
    // Could add captive portal detection here
    // For now, just monitor connected stations
    static int lastStationCount = -1;
    int stationCount = WiFi.softAPgetStationNum();

    if (stationCount != lastStationCount) {
        lastStationCount = stationCount;
        DEBUG_PRINTF("WiFiManager: AP stations connected: %d\n", stationCount);
    }
}

// Static WiFi event handler
void WiFiManager::onWiFiEvent(WiFiEvent_t event) {
    if (!instance) return;

    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            DEBUG_PRINTLN("=====================================");
            DEBUG_PRINTF("WiFiManager: Connected to %s!\n", WiFi.SSID().c_str());
            DEBUG_PRINTF("IP Address: %s\n", WiFi.localIP().toString().c_str());
            DEBUG_PRINTF("Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
            DEBUG_PRINTF("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
            DEBUG_PRINTF("DNS: %s\n", WiFi.dnsIP().toString().c_str());
            DEBUG_PRINTF("Signal Strength: %d dBm\n", WiFi.RSSI());

            // Start mDNS
            if (MDNS.begin(MDNS_HOSTNAME)) {
                DEBUG_PRINTF("mDNS started: http://%s.local\n", MDNS_HOSTNAME);
                MDNS.addService("http", "tcp", 80);
            } else {
                DEBUG_PRINTLN("mDNS failed to start!");
            }

            // Don't start web server here - WebInterface will handle it

            DEBUG_PRINTLN("=====================================");
            DEBUG_PRINTLN("Access the controller at:");
            DEBUG_PRINTF("  http://%s\n", WiFi.localIP().toString().c_str());
            DEBUG_PRINTF("  http://%s.local\n", MDNS_HOSTNAME);
            DEBUG_PRINTLN("=====================================");

            instance->setState(WiFiState::CONNECTED);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            if (instance->state == WiFiState::CONNECTED) {
                DEBUG_PRINTLN("WiFiManager: Disconnected from WiFi");
                instance->setState(WiFiState::DISCONNECTED);
                // Try to reconnect
                instance->lastReconnectAttempt = millis();
                instance->reconnectAttempts = 0;
                instance->setState(WiFiState::CONNECTION_FAILED);
            }
            break;

        case ARDUINO_EVENT_WIFI_AP_START:
            DEBUG_PRINTLN("WiFiManager: AP started");
            break;

        case ARDUINO_EVENT_WIFI_AP_STOP:
            DEBUG_PRINTLN("WiFiManager: AP stopped");
            break;

        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            DEBUG_PRINTLN("WiFiManager: Station connected to AP");
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            DEBUG_PRINTLN("WiFiManager: Station disconnected from AP");
            break;

        default:
            break;
    }
}

// DNS Server implementation for captive portal
void WiFiManager::startDNSServer() {
    if (!dnsServerRunning) {
        DEBUG_PRINTLN("WiFiManager: Starting DNS server for captive portal");

        // Start listening on port 53
        if (dnsUdp.begin(53)) {
            dnsServerRunning = true;
            DEBUG_PRINTLN("WiFiManager: DNS server started on port 53");
        } else {
            DEBUG_PRINTLN("WiFiManager: Failed to start DNS server");
        }
    }
}

void WiFiManager::stopDNSServer() {
    if (dnsServerRunning) {
        DEBUG_PRINTLN("WiFiManager: Stopping DNS server");
        dnsUdp.stop();
        dnsServerRunning = false;
    }
}

void WiFiManager::handleDNS() {
    if (!dnsServerRunning) return;

    int packetSize = dnsUdp.parsePacket();
    if (packetSize > 0) {
        // DNS request received
        uint8_t dnsRequest[packetSize];
        dnsUdp.read(dnsRequest, packetSize);

        // Prepare DNS response
        uint8_t dnsResponse[packetSize + 16];
        memcpy(dnsResponse, dnsRequest, packetSize);

        // Set response flags (standard query response, no error)
        dnsResponse[2] = 0x81;  // Standard query response, recursion available
        dnsResponse[3] = 0x80;  // No error

        // Set answer count to 1
        dnsResponse[6] = 0x00;
        dnsResponse[7] = 0x01;

        // Add answer section
        int responseLen = packetSize;

        // Answer name (pointer to question)
        dnsResponse[responseLen++] = 0xC0;
        dnsResponse[responseLen++] = 0x0C;

        // Type A (host address)
        dnsResponse[responseLen++] = 0x00;
        dnsResponse[responseLen++] = 0x01;

        // Class IN
        dnsResponse[responseLen++] = 0x00;
        dnsResponse[responseLen++] = 0x01;

        // TTL (60 seconds)
        dnsResponse[responseLen++] = 0x00;
        dnsResponse[responseLen++] = 0x00;
        dnsResponse[responseLen++] = 0x00;
        dnsResponse[responseLen++] = 0x3C;

        // Data length (4 bytes for IPv4)
        dnsResponse[responseLen++] = 0x00;
        dnsResponse[responseLen++] = 0x04;

        // IP address (10.0.0.1)
        dnsResponse[responseLen++] = 10;
        dnsResponse[responseLen++] = 0;
        dnsResponse[responseLen++] = 0;
        dnsResponse[responseLen++] = 1;

        // Send response
        dnsUdp.beginPacket(dnsUdp.remoteIP(), dnsUdp.remotePort());
        dnsUdp.write(dnsResponse, responseLen);
        dnsUdp.endPacket();
    }
}

// Web server route setup
void WiFiManager::setupWebRoutes() {
    // API routes
    webServer->on("/api/wifi/scan", [this](WiFiClient& client, const String& method, const String& query) {
        handleWiFiScan(client, method, query);
    });

    webServer->on("/api/wifi/connect", [this](WiFiClient& client, const String& method, const String& query) {
        handleWiFiConnect(client, method, query);
    });

    webServer->on("/api/wifi/status", [this](WiFiClient& client, const String& method, const String& query) {
        handleWiFiStatus(client, method, query);
    });

    // Add a simple test endpoint
    webServer->on("/api/test", [](WiFiClient& client, const String& method, const String& query) {
        SimpleHTTPServer::sendJSON(client, "{\"status\":\"ok\",\"message\":\"Web server is running!\"}");
    });
}

// Handle WiFi scan request
void WiFiManager::handleWiFiScan(WiFiClient& client, const String& method, const String& query) {
    DEBUG_PRINTLN("WiFiManager: Handling WiFi scan request");

    // Perform WiFi scan
    int n = WiFi.scanNetworks();

    // Build JSON response
    DynamicJsonDocument doc(2048);
    JsonArray networks = doc.createNestedArray("networks");

    for (int i = 0; i < n; i++) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encryption"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    String response;
    serializeJson(doc, response);

    SimpleHTTPServer::sendJSON(client, response);
}

// Handle WiFi connect request
void WiFiManager::handleWiFiConnect(WiFiClient& client, const String& method, const String& query) {
    if (method != "POST") {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        return;
    }

    // Read POST body
    String body = "";
    while (client.available()) {
        body += (char)client.read();
    }

    DEBUG_PRINTF("WiFiManager: Connect request body: %s\n", body.c_str());

    // Parse JSON
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        SimpleHTTPServer::sendJSON(client, "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    String ssid = doc["ssid"] | "";
    String password = doc["password"] | "";

    if (ssid.length() == 0) {
        SimpleHTTPServer::sendJSON(client, "{\"success\":false,\"error\":\"SSID required\"}");
        return;
    }

    // Save credentials
    saveCredentials(ssid, password);

    // Send immediate response with connection info
    String response = "{\"success\":true,\"message\":\"Credentials saved. Device will connect and be available at:\",";
    response += "\"hostname\":\"" + String(MDNS_HOSTNAME) + ".local\",";
    response += "\"info\":\"Check serial monitor for IP address\"}";

    SimpleHTTPServer::sendJSON(client, response);

    // Schedule connection after delay
    connectStartTime = millis() + 2000; // Give client time to receive response

    // Actually initiate the connection after delay
    connect();
}

// Handle WiFi status request
void WiFiManager::handleWiFiStatus(WiFiClient& client, const String& method, const String& query) {
    DynamicJsonDocument doc(256);

    doc["configured"] = credentials.valid;
    doc["connected"] = isConnected();
    doc["ssid"] = credentials.valid ? String(credentials.ssid) : "";
    doc["ip"] = isConnected() ? WiFi.localIP().toString() : "";
    doc["rssi"] = isConnected() ? WiFi.RSSI() : 0;

    String response;
    serializeJson(doc, response);

    SimpleHTTPServer::sendJSON(client, response);
}