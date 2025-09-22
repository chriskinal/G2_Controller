#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include "SimpleHTTPServer.h"
#include "SimpleWebSocket.h"
#include "ModbusVFD.h"
#include <ArduinoJson.h>

class WebInterface {
public:
    WebInterface(ModbusVFD& vfd);
    ~WebInterface();

    // Initialize web server and websocket
    bool begin();

    // Handle incoming requests (call from loop)
    void handle();

    // Update VFD status and broadcast to clients
    void updateStatus();

private:
    SimpleHTTPServer httpServer;
    SimpleWebSocketServer wsServer;
    ModbusVFD& vfd;

    unsigned long lastStatusUpdate;
    unsigned long lastVFDUpdate;

    // Setup HTTP routes
    void setupRoutes();

    // HTTP handlers
    void handleVFDStatus(WiFiClient& client, const String& method, const String& query);
    void handleVFDStart(WiFiClient& client, const String& method, const String& query);
    void handleVFDStop(WiFiClient& client, const String& method, const String& query);
    void handleVFDFrequency(WiFiClient& client, const String& method, const String& query);
    void handleSettings(WiFiClient& client, const String& method, const String& query);

    // WebSocket message handler
    void handleWebSocketMessage(WebSocketClient* client, const uint8_t* data, size_t length, bool isText);

    // Helper to build status JSON
    String buildStatusJSON();

    // Parse JSON body from POST request
    bool parseJSONBody(WiFiClient& client, DynamicJsonDocument& doc);
};

#endif // WEB_INTERFACE_H