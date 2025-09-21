// SimpleHTTPServer.h
// Lightweight HTTP server implementation adapted for ESP32
// Based on AiO_New_Dawn SimpleHTTPServer

#ifndef SIMPLE_HTTP_SERVER_H
#define SIMPLE_HTTP_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <functional>
#include <vector>

// Route handler function type
using HTTPHandler = std::function<void(WiFiClient&, const String&, const String&)>;

// Simple HTTP server optimized for ESP32
class SimpleHTTPServer {
public:
    SimpleHTTPServer();
    ~SimpleHTTPServer();

    // Server control
    bool begin(uint16_t port = 80);
    void stop();
    void handleClient();

    // Route registration
    void on(const String& path, HTTPHandler handler);

    // Server info
    bool isRunning() const { return running; }
    uint16_t getPort() const { return serverPort; }

    // Helper methods for responses
    static void send(WiFiClient& client, int code, const String& contentType, const String& content);
    static void sendJSON(WiFiClient& client, const String& json);
    static void redirect(WiFiClient& client, const String& location);
    static void sendFile(WiFiClient& client, const String& path);

private:
    struct Route {
        String path;
        HTTPHandler handler;
    };

    WiFiServer server;
    std::vector<Route> routes;
    uint16_t serverPort;
    bool running;

    // Request parsing
    bool parseRequest(WiFiClient& client, String& method, String& path, String& query);

    // Route matching
    Route* findRoute(const String& path);

    // Default handlers
    void handleNotFound(WiFiClient& client);

    // Helper methods
    static String getContentType(const String& path);
    static String urlDecode(const String& str);
};

#endif // SIMPLE_HTTP_SERVER_H