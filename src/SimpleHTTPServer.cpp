// SimpleHTTPServer.cpp
// Lightweight HTTP server implementation adapted for ESP32

#include "SimpleHTTPServer.h"
#include "Config.h"
#include <SPIFFS.h>

SimpleHTTPServer::SimpleHTTPServer() : server(80), serverPort(80), running(false) {
}

SimpleHTTPServer::~SimpleHTTPServer() {
    stop();
}

bool SimpleHTTPServer::begin(uint16_t port) {
    serverPort = port;
    server = WiFiServer(port);
    server.begin();
    running = true;

    DEBUG_PRINTF("SimpleHTTPServer: Started on port %d\n", port);

    // Initialize SPIFFS if not already done
    if (!SPIFFS.begin(true)) {
        DEBUG_PRINTLN("SimpleHTTPServer: Failed to mount SPIFFS");
        return false;
    }

    return true;
}

void SimpleHTTPServer::stop() {
    if (running) {
        server.stop();
        running = false;
        DEBUG_PRINTLN("SimpleHTTPServer: Stopped");
    }
}

void SimpleHTTPServer::handleClient() {
    if (!running) return;

    WiFiClient client = server.available();
    if (!client) return;

    // Wait for data with timeout
    unsigned long timeout = millis() + 5000;
    while (!client.available() && millis() < timeout) {
        delay(1);
    }

    if (!client.available()) {
        client.stop();
        return;
    }

    String method, path, query;
    if (!parseRequest(client, method, path, query)) {
        client.stop();
        return;
    }

    DEBUG_PRINTF("SimpleHTTPServer: %s %s\n", method.c_str(), path.c_str());

    // Check for registered routes
    Route* route = findRoute(path);
    if (route) {
        route->handler(client, method, query);
    } else {
        // Try to serve file from SPIFFS
        sendFile(client, path);
    }

    // Give client time to receive data
    delay(1);
    client.stop();
}

void SimpleHTTPServer::on(const String& path, HTTPHandler handler) {
    routes.push_back({path, handler});
    DEBUG_PRINTF("SimpleHTTPServer: Route added: %s\n", path.c_str());
}

bool SimpleHTTPServer::parseRequest(WiFiClient& client, String& method, String& path, String& query) {
    String requestLine = client.readStringUntil('\n');
    requestLine.trim();

    int firstSpace = requestLine.indexOf(' ');
    int secondSpace = requestLine.indexOf(' ', firstSpace + 1);

    if (firstSpace == -1 || secondSpace == -1) {
        return false;
    }

    method = requestLine.substring(0, firstSpace);
    String fullPath = requestLine.substring(firstSpace + 1, secondSpace);

    // Extract path and query string
    int queryStart = fullPath.indexOf('?');
    if (queryStart != -1) {
        path = fullPath.substring(0, queryStart);
        query = fullPath.substring(queryStart + 1);
    } else {
        path = fullPath;
        query = "";
    }

    // Skip headers
    while (client.available()) {
        String header = client.readStringUntil('\n');
        header.trim();
        if (header.length() == 0) break;
    }

    return true;
}

SimpleHTTPServer::Route* SimpleHTTPServer::findRoute(const String& path) {
    for (auto& route : routes) {
        if (route.path == path) {
            return &route;
        }
    }
    return nullptr;
}

void SimpleHTTPServer::send(WiFiClient& client, int code, const String& contentType, const String& content) {
    String status;
    switch (code) {
        case 200: status = "OK"; break;
        case 302: status = "Found"; break;
        case 404: status = "Not Found"; break;
        case 500: status = "Internal Server Error"; break;
        default: status = "Unknown"; break;
    }

    client.printf("HTTP/1.1 %d %s\r\n", code, status.c_str());
    client.printf("Content-Type: %s\r\n", contentType.c_str());
    client.printf("Content-Length: %d\r\n", content.length());
    client.println("Connection: close\r\n");
    client.print(content);
}

void SimpleHTTPServer::sendJSON(WiFiClient& client, const String& json) {
    send(client, 200, "application/json", json);
}

void SimpleHTTPServer::redirect(WiFiClient& client, const String& location) {
    client.println("HTTP/1.1 302 Found");
    client.printf("Location: %s\r\n", location.c_str());
    client.println("Connection: close\r\n");
}

void SimpleHTTPServer::sendFile(WiFiClient& client, const String& path) {
    String filePath = path;

    // Default to index.html for root
    if (filePath == "/") {
        filePath = "/index.html";
    }

    // Check for WiFi config page specifically
    if (filePath == "/wifi" || filePath == "/wifi_config") {
        filePath = "/wifi_config.html";
    }

    if (!SPIFFS.exists(filePath)) {
        // For captive portal, redirect all 404s to the config page
        redirect(client, "/wifi_config.html");
        return;
    }

    File file = SPIFFS.open(filePath, "r");
    if (!file) {
        send(client, 500, "text/plain", "Failed to open file");
        return;
    }

    String contentType = getContentType(filePath);
    client.println("HTTP/1.1 200 OK");
    client.printf("Content-Type: %s\r\n", contentType.c_str());
    client.printf("Content-Length: %d\r\n", file.size());
    client.println("Connection: close");
    client.println();

    // Send file in chunks
    uint8_t buffer[1024];
    while (file.available()) {
        size_t len = file.read(buffer, sizeof(buffer));
        client.write(buffer, len);
    }

    file.close();
}

void SimpleHTTPServer::handleNotFound(WiFiClient& client) {
    // For captive portal, redirect all 404s to the config page
    redirect(client, "/wifi_config.html");
}

String SimpleHTTPServer::getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

String SimpleHTTPServer::urlDecode(const String& str) {
    String decoded = "";
    char temp[] = "0x00";

    for (unsigned int i = 0; i < str.length(); i++) {
        if (str[i] == '+') {
            decoded += ' ';
        } else if (str[i] == '%' && i + 2 < str.length()) {
            temp[2] = str[i + 1];
            temp[3] = str[i + 2];
            decoded += (char)strtol(temp, NULL, 16);
            i += 2;
        } else {
            decoded += str[i];
        }
    }

    return decoded;
}