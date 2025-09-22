// SimpleWebSocket.cpp
// Lightweight WebSocket server implementation for ESP32

#include "SimpleWebSocket.h"
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>

// WebSocket GUID as per RFC 6455
static const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Static member initialization
uint32_t WebSocketClient::nextClientId = 1;

// ===== WebSocketClient Implementation =====

WebSocketClient::WebSocketClient(WiFiClient& client) :
    tcpClient(client),
    clientId(nextClientId++),
    handshakeComplete(false),
    lastPingTime(millis()),
    lastPongTime(millis())
{
    receiveBuffer.reserve(1024);
    DEBUG_PRINTF("WebSocket: Created client %d, TCP connected: %d\n",
                 clientId, tcpClient.connected());
}

WebSocketClient::~WebSocketClient() {
    if (tcpClient.connected()) {
        close();
    }
}

bool WebSocketClient::isConnected() const {
    bool tcpConnected = const_cast<WiFiClient&>(tcpClient).connected();

    // For new connections that haven't completed handshake,
    // we're still "connected" as long as TCP is alive
    if (tcpConnected && !handshakeComplete) {
        return true;  // Still connected, handshake pending
    }

    // For established connections, both TCP and handshake must be valid
    return tcpConnected && handshakeComplete;
}

bool WebSocketClient::poll() {
    if (!tcpClient.connected()) {
        DEBUG_PRINTF("WebSocket: Client %d - TCP disconnected in poll\n", clientId);
        return false;
    }

    // Handle handshake if not complete
    if (!handshakeComplete) {
        DEBUG_PRINTF("WebSocket: Client %d - Calling performHandshake\n", clientId);
        bool result = performHandshake();
        DEBUG_PRINTF("WebSocket: Client %d - performHandshake returned %d\n", clientId, result);
        return result;
    }

    // Read incoming data
    while (tcpClient.available()) {
        WSFrameHeader header;
        std::vector<uint8_t> payload;

        if (readFrame(header, payload)) {
            processFrame(header, payload);
        } else {
            // Frame read error
            close();
            return false;
        }
    }

    // Send ping every 30 seconds
    if (millis() - lastPingTime > 30000) {
        sendPing();
        lastPingTime = millis();
    }

    // Check for pong timeout (60 seconds)
    if (millis() - lastPongTime > 60000) {
        DEBUG_PRINTLN("WebSocket: Ping timeout, closing connection");
        close();
        return false;
    }

    return true;
}

bool WebSocketClient::performHandshake() {
    if (!tcpClient.available()) {
        return true; // Still waiting for data
    }

    DEBUG_PRINTF("WebSocket: Client %d starting handshake\n", clientId);

    String request = "";
    unsigned long timeout = millis() + 1000; // 1 second timeout

    while (millis() < timeout && request.indexOf("\r\n\r\n") == -1) {
        if (tcpClient.available()) {
            char c = tcpClient.read();
            request += c;
            if (request.length() > 2048) {
                DEBUG_PRINTF("WebSocket: Client %d - Request too large\n", clientId);
                return false;
            }
        } else {
            delay(1);
        }
    }

    if (request.indexOf("\r\n\r\n") == -1) {
        DEBUG_PRINTF("WebSocket: Client %d - Incomplete request\n", clientId);
        return false;
    }

    // Extract WebSocket key
    String key = extractWebSocketKey(request);
    if (key.length() == 0) {
        DEBUG_PRINTF("WebSocket: Client %d - No WebSocket key found\n", clientId);
        DEBUG_PRINTF("WebSocket: Request:\n%s\n", request.c_str());
        return false;
    }

    DEBUG_PRINTF("WebSocket: Client %d - Key: %s\n", clientId, key.c_str());

    // Generate accept key
    String acceptKey = generateAcceptKey(key);

    // Send handshake response
    String response = "HTTP/1.1 101 Switching Protocols\r\n";
    response += "Upgrade: websocket\r\n";
    response += "Connection: Upgrade\r\n";
    response += "Sec-WebSocket-Accept: " + acceptKey + "\r\n";
    response += "\r\n";

    size_t written = tcpClient.write(response.c_str(), response.length());
    tcpClient.flush();

    DEBUG_PRINTF("WebSocket: Client %d - Sent %d bytes of handshake response\n", clientId, written);

    handshakeComplete = true;
    lastPongTime = millis();

    DEBUG_PRINTF("WebSocket: Client %d handshake complete\n", clientId);
    DEBUG_PRINTF("WebSocket: Accept key sent: %s\n", acceptKey.c_str());

    return true;
}

String WebSocketClient::extractWebSocketKey(const String& request) {
    int keyIndex = request.indexOf("Sec-WebSocket-Key: ");
    if (keyIndex == -1) {
        return "";
    }

    keyIndex += 19; // Length of "Sec-WebSocket-Key: "
    int keyEnd = request.indexOf("\r\n", keyIndex);
    if (keyEnd == -1) {
        return "";
    }

    return request.substring(keyIndex, keyEnd);
}

String WebSocketClient::generateAcceptKey(const String& key) {
    String acceptKey = key + WS_GUID;

    // SHA1 hash
    uint8_t hash[20];
    mbedtls_sha1_context ctx;
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);
    mbedtls_sha1_update(&ctx, (uint8_t*)acceptKey.c_str(), acceptKey.length());
    mbedtls_sha1_finish(&ctx, hash);
    mbedtls_sha1_free(&ctx);

    // Base64 encode
    size_t outputLen = 0;
    mbedtls_base64_encode(nullptr, 0, &outputLen, hash, 20);

    char output[outputLen + 1];
    mbedtls_base64_encode((unsigned char*)output, outputLen + 1, &outputLen, hash, 20);
    output[outputLen] = '\0';

    return String(output);
}

bool WebSocketClient::readFrame(WSFrameHeader& header, std::vector<uint8_t>& payload) {
    if (!tcpClient.available()) {
        return false;
    }

    // Read first byte
    uint8_t byte1 = tcpClient.read();
    header.fin = (byte1 & 0x80) != 0;
    header.rsv1 = (byte1 & 0x40) != 0;
    header.rsv2 = (byte1 & 0x20) != 0;
    header.rsv3 = (byte1 & 0x10) != 0;
    header.opcode = static_cast<WSOpcode>(byte1 & 0x0F);

    // Read second byte
    if (!tcpClient.available()) return false;
    uint8_t byte2 = tcpClient.read();
    header.masked = (byte2 & 0x80) != 0;
    header.payloadLength = byte2 & 0x7F;

    // Read extended payload length if needed
    if (header.payloadLength == 126) {
        if (tcpClient.available() < 2) return false;
        header.payloadLength = ((uint16_t)tcpClient.read() << 8) | tcpClient.read();
    } else if (header.payloadLength == 127) {
        if (tcpClient.available() < 8) return false;
        header.payloadLength = 0;
        for (int i = 0; i < 8; i++) {
            header.payloadLength = (header.payloadLength << 8) | tcpClient.read();
        }
    }

    // Read mask key if present
    if (header.masked) {
        if (tcpClient.available() < 4) return false;
        for (int i = 0; i < 4; i++) {
            header.maskKey[i] = tcpClient.read();
        }
    }

    // Read payload
    payload.resize(header.payloadLength);
    size_t bytesRead = 0;
    while (bytesRead < header.payloadLength) {
        if (!tcpClient.available()) {
            delay(1);
            continue;
        }
        int available = tcpClient.available();
        int toRead = min((int)(header.payloadLength - bytesRead), available);
        bytesRead += tcpClient.read(&payload[bytesRead], toRead);
    }

    // Unmask payload if needed
    if (header.masked) {
        for (size_t i = 0; i < payload.size(); i++) {
            payload[i] ^= header.maskKey[i % 4];
        }
    }

    return true;
}

bool WebSocketClient::sendFrame(WSOpcode opcode, const uint8_t* data, size_t length) {
    if (!isConnected()) return false;

    std::vector<uint8_t> frame;
    frame.reserve(length + 10);

    // First byte: FIN + opcode
    frame.push_back(0x80 | static_cast<uint8_t>(opcode));

    // Payload length
    if (length < 126) {
        frame.push_back(length);
    } else if (length < 65536) {
        frame.push_back(126);
        frame.push_back((length >> 8) & 0xFF);
        frame.push_back(length & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back((length >> (i * 8)) & 0xFF);
        }
    }

    // Payload
    frame.insert(frame.end(), data, data + length);

    // Send frame
    size_t sent = tcpClient.write(frame.data(), frame.size());

    return sent == frame.size();
}

void WebSocketClient::processFrame(const WSFrameHeader& header, const std::vector<uint8_t>& payload) {
    switch (header.opcode) {
        case WSOpcode::TEXT:
        case WSOpcode::BINARY:
            if (messageCallback) {
                messageCallback(payload.data(), payload.size(), header.opcode == WSOpcode::TEXT);
            }
            break;

        case WSOpcode::CLOSE:
            close();
            break;

        case WSOpcode::PING:
            // Send pong
            sendFrame(WSOpcode::PONG, payload.data(), payload.size());
            break;

        case WSOpcode::PONG:
            lastPongTime = millis();
            break;

        default:
            break;
    }
}

bool WebSocketClient::sendText(const String& text) {
    bool result = sendFrame(WSOpcode::TEXT, (uint8_t*)text.c_str(), text.length());
    if (!result) {
        DEBUG_PRINTF("WebSocket: Failed to send text frame (client %d)\n", clientId);
    }
    return result;
}

bool WebSocketClient::sendBinary(const uint8_t* data, size_t length) {
    return sendFrame(WSOpcode::BINARY, data, length);
}

bool WebSocketClient::sendPing() {
    return sendFrame(WSOpcode::PING, nullptr, 0);
}

void WebSocketClient::close(uint16_t code, const String& reason) {
    if (isConnected()) {
        std::vector<uint8_t> payload;
        payload.push_back((code >> 8) & 0xFF);
        payload.push_back(code & 0xFF);
        if (reason.length() > 0) {
            payload.insert(payload.end(), reason.begin(), reason.end());
        }
        sendFrame(WSOpcode::CLOSE, payload.data(), payload.size());
    }

    tcpClient.stop();
    handshakeComplete = false;

    if (closeCallback) {
        closeCallback();
    }
}

// ===== SimpleWebSocketServer Implementation =====

SimpleWebSocketServer::SimpleWebSocketServer() :
    server(0),  // Don't initialize with port yet
    maxClients(4),
    running(false),
    serverPort(0)
{
}

SimpleWebSocketServer::SimpleWebSocketServer(uint16_t port) :
    server(port),
    maxClients(4),
    running(false),
    serverPort(port)
{
}

SimpleWebSocketServer::~SimpleWebSocketServer() {
    stop();
}

bool SimpleWebSocketServer::begin(uint16_t port) {
    if (running) {
        DEBUG_PRINTLN("SimpleWebSocketServer: Already running");
        return false;
    }

    serverPort = port;

    // Always create new server instance
    server = WiFiServer(port);

    server.begin();
    server.setNoDelay(true);

    running = true;

    DEBUG_PRINTF("SimpleWebSocketServer: Started on port %d\n", port);

    // Verify server is listening
    delay(100);
    WiFiClient testClient = server.available();
    if (testClient) {
        testClient.stop();
    }

    return true;
}

void SimpleWebSocketServer::stop() {
    if (running) {
        running = false;
        server.stop();

        // Close all clients
        for (auto& client : clients) {
            client->close();
        }
        clients.clear();

        DEBUG_PRINTLN("SimpleWebSocketServer: Stopped");
    }
}

void SimpleWebSocketServer::handleClients() {
    if (!running) return;

    acceptNewClients();
    removeDisconnectedClients();

    // Poll each client
    for (auto& client : clients) {
        bool pollResult = client->poll();
        if (!pollResult) {
            DEBUG_PRINTF("SimpleWebSocketServer: Client %d poll failed\n", client->getClientId());
        }
    }
}

void SimpleWebSocketServer::acceptNewClients() {
    WiFiClient newClient = server.available();
    if (!newClient) return;

    DEBUG_PRINTLN("SimpleWebSocketServer: New TCP connection");

    if (clients.size() >= maxClients) {
        DEBUG_PRINTLN("SimpleWebSocketServer: Max clients reached, rejecting connection");
        newClient.stop();
        return;
    }

    // Small delay to ensure connection is stable
    delay(10);

    if (!newClient.connected()) {
        DEBUG_PRINTLN("SimpleWebSocketServer: TCP client disconnected immediately");
        newClient.stop();
        return;
    }

    std::unique_ptr<WebSocketClient> wsClient(new WebSocketClient(newClient));

    // Set message handler
    if (messageHandler) {
        WebSocketClient* clientPtr = wsClient.get();
        wsClient->onMessage([this, clientPtr](const uint8_t* data, size_t length, bool isText) {
            messageHandler(clientPtr, data, length, isText);
        });
    }

    clients.push_back(std::move(wsClient));

    DEBUG_PRINTF("SimpleWebSocketServer: New client connected (total: %d)\n", clients.size());
}

void SimpleWebSocketServer::removeDisconnectedClients() {
    size_t beforeCount = clients.size();
    clients.erase(
        std::remove_if(clients.begin(), clients.end(),
            [](const std::unique_ptr<WebSocketClient>& client) {
                bool connected = client->isConnected();
                if (!connected) {
                    DEBUG_PRINTF("SimpleWebSocketServer: Client %d disconnected\n", client->getClientId());
                }
                return !connected;
            }),
        clients.end()
    );
    if (beforeCount != clients.size()) {
        DEBUG_PRINTF("SimpleWebSocketServer: Removed %d clients, %d remaining\n",
                     beforeCount - clients.size(), clients.size());
    }
}

size_t SimpleWebSocketServer::getClientCount() const {
    return clients.size();
}

void SimpleWebSocketServer::broadcastText(const String& text) {
    for (auto& client : clients) {
        client->sendText(text);
    }
}

void SimpleWebSocketServer::broadcastBinary(const uint8_t* data, size_t length) {
    for (auto& client : clients) {
        client->sendBinary(data, length);
    }
}