#include "WebInterface.h"
#include "Config.h"

WebInterface::WebInterface(ModbusVFD& vfd) :
    vfd(vfd),
    lastStatusUpdate(0),
    lastVFDUpdate(0)
{
}

WebInterface::~WebInterface() {
    wsServer.stop();
    httpServer.stop();
}

bool WebInterface::begin() {
    // Setup HTTP routes
    setupRoutes();

    // Start HTTP server
    if (!httpServer.begin(WEB_SERVER_PORT)) {
        DEBUG_PRINTLN("WebInterface: Failed to start HTTP server");
        return false;
    }

    // Start WebSocket server
    if (!wsServer.begin(WS_PORT)) {
        DEBUG_PRINTLN("WebInterface: Failed to start WebSocket server");
        return false;
    }

    // Setup WebSocket message handler
    wsServer.onMessage([this](WebSocketClient* client, const uint8_t* data, size_t length, bool isText) {
        handleWebSocketMessage(client, data, length, isText);
    });

    DEBUG_PRINTLN("WebInterface: Started successfully");
    DEBUG_PRINTF("  HTTP server on port %d\n", WEB_SERVER_PORT);
    DEBUG_PRINTF("  WebSocket server on port %d\n", WS_PORT);

    return true;
}

void WebInterface::handle() {
    // Handle HTTP requests
    httpServer.handleClient();

    // Handle WebSocket connections
    wsServer.handleClients();

    // Update VFD status periodically
    unsigned long now = millis();
    if (now - lastVFDUpdate >= 100) {  // Update VFD every 100ms
        lastVFDUpdate = now;
        vfd.updateStatus();
    }

    // Broadcast status to WebSocket clients
    if (now - lastStatusUpdate >= 250) {  // Broadcast every 250ms
        lastStatusUpdate = now;
        updateStatus();
    }
}

void WebInterface::updateStatus() {
    String status = buildStatusJSON();
    size_t clientCount = wsServer.getClientCount();
    if (clientCount > 0) {
        DEBUG_PRINTF("WebInterface: Broadcasting to %d clients\n", clientCount);
        wsServer.broadcastText(status);
    }
}

String WebInterface::buildStatusJSON() {
    StaticJsonDocument<256> doc;

    doc["connected"] = vfd.isConnected();
    doc["running"] = vfd.isRunning();
    doc["fault"] = vfd.isFaulted();
    doc["frequency"] = vfd.getFrequency();
    doc["target"] = vfd.getTargetFrequency();
    doc["current"] = vfd.getCurrent();
    doc["voltage"] = vfd.getVoltage();
    doc["statusWord"] = vfd.getStatusWord();

    String output;
    serializeJson(doc, output);
    return output;
}

void WebInterface::setupRoutes() {
    // VFD status endpoint
    httpServer.on("/api/vfd/status", [this](WiFiClient& client, const String& method, const String& query) {
        handleVFDStatus(client, method, query);
    });

    // VFD control endpoints
    httpServer.on("/api/vfd/start", [this](WiFiClient& client, const String& method, const String& query) {
        handleVFDStart(client, method, query);
    });

    httpServer.on("/api/vfd/stop", [this](WiFiClient& client, const String& method, const String& query) {
        handleVFDStop(client, method, query);
    });

    httpServer.on("/api/vfd/frequency", [this](WiFiClient& client, const String& method, const String& query) {
        handleVFDFrequency(client, method, query);
    });

    // Settings endpoint
    httpServer.on("/api/settings", [this](WiFiClient& client, const String& method, const String& query) {
        handleSettings(client, method, query);
    });

    // WebSocket test endpoint
    httpServer.on("/api/wstest", [this](WiFiClient& client, const String& method, const String& query) {
        StaticJsonDocument<256> doc;
        doc["wsPort"] = WS_PORT;
        doc["wsClients"] = wsServer.getClientCount();
        doc["running"] = true;
        String response;
        serializeJson(doc, response);
        SimpleHTTPServer::sendJSON(client, response);
    });
}

void WebInterface::handleVFDStatus(WiFiClient& client, const String& method, const String& query) {
    String status = buildStatusJSON();
    SimpleHTTPServer::sendJSON(client, status);
}

void WebInterface::handleVFDStart(WiFiClient& client, const String& method, const String& query) {
    if (method != "POST") {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        return;
    }

    bool success = vfd.start();

    StaticJsonDocument<128> doc;
    doc["success"] = success;
    doc["message"] = success ? "VFD started" : "Failed to start VFD";

    String response;
    serializeJson(doc, response);
    SimpleHTTPServer::sendJSON(client, response);

    // Force immediate status update
    updateStatus();
}

void WebInterface::handleVFDStop(WiFiClient& client, const String& method, const String& query) {
    if (method != "POST") {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
        return;
    }

    bool success = vfd.stop();

    StaticJsonDocument<128> doc;
    doc["success"] = success;
    doc["message"] = success ? "VFD stopped" : "Failed to stop VFD";

    String response;
    serializeJson(doc, response);
    SimpleHTTPServer::sendJSON(client, response);

    // Force immediate status update
    updateStatus();
}

void WebInterface::handleVFDFrequency(WiFiClient& client, const String& method, const String& query) {
    if (method == "GET") {
        // Return current frequency
        StaticJsonDocument<128> doc;
        doc["frequency"] = vfd.getFrequency();
        doc["target"] = vfd.getTargetFrequency();

        String response;
        serializeJson(doc, response);
        SimpleHTTPServer::sendJSON(client, response);

    } else if (method == "POST") {
        // Set new frequency
        DynamicJsonDocument doc(256);
        if (!parseJSONBody(client, doc)) {
            SimpleHTTPServer::sendJSON(client, "{\"success\":false,\"error\":\"Invalid JSON\"}");
            return;
        }

        float frequency = doc["frequency"] | -1.0;
        DEBUG_PRINTF("WebInterface: Parsed frequency: %.2f\n", frequency);

        if (frequency < 0) {
            SimpleHTTPServer::sendJSON(client, "{\"success\":false,\"error\":\"Missing frequency parameter\"}");
            return;
        }

        // Get VFD parameters
        VFDParams params = vfd.getParameters();
        if (frequency < params.minFrequency || frequency > params.maxFrequency) {
            SimpleHTTPServer::sendJSON(client, "{\"success\":false,\"error\":\"Frequency out of range\"}");
            return;
        }

        bool success = vfd.setFrequency(frequency);

        StaticJsonDocument<128> response;
        response["success"] = success;
        response["message"] = success ? "Frequency set" : "Failed to set frequency";
        response["frequency"] = frequency;

        String output;
        serializeJson(response, output);
        SimpleHTTPServer::sendJSON(client, output);

        // Force immediate status update
        updateStatus();

    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void WebInterface::handleSettings(WiFiClient& client, const String& method, const String& query) {
    if (method == "GET") {
        // Return current settings
        VFDParams params = vfd.getParameters();

        StaticJsonDocument<256> doc;
        doc["minFrequency"] = params.minFrequency;
        doc["maxFrequency"] = params.maxFrequency;
        doc["rampUpTime"] = params.rampUpTime;
        doc["rampDownTime"] = params.rampDownTime;
        doc["slaveId"] = MODBUS_SLAVE_ID;
        doc["baudRate"] = RS485_BAUD_RATE;

        String response;
        serializeJson(doc, response);
        SimpleHTTPServer::sendJSON(client, response);

    } else if (method == "POST") {
        // Update settings
        DynamicJsonDocument doc(512);
        if (!parseJSONBody(client, doc)) {
            SimpleHTTPServer::sendJSON(client, "{\"success\":false,\"error\":\"Invalid JSON\"}");
            return;
        }

        VFDParams params = vfd.getParameters();
        params.minFrequency = doc["minFrequency"] | params.minFrequency;
        params.maxFrequency = doc["maxFrequency"] | params.maxFrequency;
        params.rampUpTime = doc["rampUpTime"] | params.rampUpTime;
        params.rampDownTime = doc["rampDownTime"] | params.rampDownTime;

        vfd.setParameters(params);

        SimpleHTTPServer::sendJSON(client, "{\"success\":true,\"message\":\"Settings updated\"}");

    } else {
        SimpleHTTPServer::send(client, 405, "text/plain", "Method Not Allowed");
    }
}

void WebInterface::handleWebSocketMessage(WebSocketClient* client, const uint8_t* data, size_t length, bool isText) {
    if (!isText) return;

    String message((char*)data, length);
    DEBUG_PRINTF("WebInterface: WebSocket message: %s\n", message.c_str());

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        client->sendText("{\"error\":\"Invalid JSON\"}");
        return;
    }

    String cmd = doc["cmd"] | "";

    if (cmd == "start") {
        bool success = vfd.start();
        client->sendText(success ? "{\"status\":\"started\"}" : "{\"error\":\"Failed to start\"}");
    } else if (cmd == "stop") {
        bool success = vfd.stop();
        client->sendText(success ? "{\"status\":\"stopped\"}" : "{\"error\":\"Failed to stop\"}");
    } else if (cmd == "setFreq") {
        float freq = doc["frequency"] | -1.0;
        if (freq >= 0) {
            bool success = vfd.setFrequency(freq);
            client->sendText(success ? "{\"status\":\"frequency set\"}" : "{\"error\":\"Failed to set frequency\"}");
        }
    } else if (cmd == "getStatus") {
        client->sendText(buildStatusJSON());
    }

    // Force immediate status update to all clients
    updateStatus();
}

bool WebInterface::parseJSONBody(WiFiClient& client, DynamicJsonDocument& doc) {
    String body = "";
    unsigned long timeout = millis() + 1000;

    // Wait a bit for body to arrive
    delay(10);

    while (millis() < timeout) {
        while (client.available()) {
            char c = client.read();
            body += c;
            if (body.length() > 1024) {
                DEBUG_PRINTLN("WebInterface: Body too large");
                return false; // Body too large
            }
        }
        if (body.length() > 0 && (body.indexOf('}') != -1 || body.indexOf(']') != -1)) {
            break;
        }
        delay(1);
    }

    DEBUG_PRINTF("WebInterface: Received body: %s\n", body.c_str());

    DeserializationError error = deserializeJson(doc, body);
    return !error;
}