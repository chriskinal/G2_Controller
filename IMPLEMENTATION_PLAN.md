# G20 VFD Web Controller Implementation Plan

## Project Overview
Create a web-based interface for the G20 VFD using ESP32-S3 with RS485/Modbus-RTU communication.

## Development Milestones

### **Milestone 1: RS485/Modbus-RTU Communication Layer**
- Set up RS485 hardware interface using existing pins (TX:17, RX:18, DE:21)
- Implement Modbus-RTU master protocol
- Create G20 VFD communication class with:
  - Read frequency command (Function 03, Address 0x0001)
  - Write frequency command (Function 06, Address 0x0001)
  - Read status/run state
  - Note: Based on g20_ch5_serial_comms.pdf - default 9600 baud, 8N1, RTU mode
- Test with hardware loopback
- **Deliverable**: Working RS485 communication, can read/write to G20

### **Milestone 2: WiFi Configuration System**
- Implement dual-mode WiFi (AP → STA)
- Create WiFi configuration storage in SPIFFS/Preferences
- AP mode: SSID "G20_Controller_Setup", captive portal
- Web form for WiFi credentials entry
- Auto-switch to STA mode after configuration
- **Deliverable**: Device connects to configured WiFi network

### **Milestone 3: Web Server Foundation**
- Set up async web server (ESPAsyncWebServer)
- Create basic HTML/CSS/JS structure
- Implement WebSocket for real-time updates
- Serve static files from SPIFFS
- **Deliverable**: Basic web page accessible via browser

### **Milestone 4: VFD Control Interface**
- Display current frequency (live updates via WebSocket)
- UP/DOWN buttons with configurable step size
- Start/Stop control
- Status indicators (Running/Stopped/Fault)
- Responsive design for mobile/desktop
- **Deliverable**: Fully functional VFD control via web interface

### **Milestone 5: OTA Update System**
- Adapt SimpleOTAHandler from AiO project for ESP32
- Implement ESP32 Update library integration
- Create web interface for firmware upload (.bin files)
- Add update progress indicator via WebSocket
- Implement safety checks (file validation, rollback on failure)
- Password protection for update page
- **Deliverable**: Secure OTA update capability via web interface

### **Milestone 6: Advanced Features & Polish**
- Add frequency presets
- Implement ramping/acceleration control
- Error handling and fault display
- Settings page (Modbus parameters, frequency limits)
- System info page (version, uptime, network status)
- **Deliverable**: Production-ready controller

## Technical Stack
- **Framework**: Arduino (PlatformIO)
- **Libraries**:
  - ModbusMaster (RS485 communication)
  - Custom SimpleHTTPServer (adapted from AiO project for ESP32)
  - Custom SimpleWebSocket (adapted from AiO project for ESP32)
  - Preferences (WiFi credential storage)
  - ArduinoJson (Data serialization)
  - Update (ESP32 OTA functionality)
  - WiFi (ESP32 WiFi library)

## Testing Strategy
- Each milestone includes hardware testing
- Use serial monitor for debug output
- Test with actual G20 VFD or Modbus simulator
- Web interface testing on multiple devices
- OTA update testing with rollback scenarios

## File Structure
```
src/
├── main.cpp              # Main application entry
├── ModbusVFD.cpp/h      # G20 VFD communication
├── WiFiManager.cpp/h     # WiFi AP/STA management
├── SimpleHTTPServer.cpp/h    # Custom HTTP server (adapted from AiO)
├── SimpleWebSocket.cpp/h     # Custom WebSocket (adapted from AiO)
├── SimpleOTAHandler.cpp/h    # OTA handler (adapted from AiO)
├── WebInterface.cpp/h        # Web routes and handlers
└── Config.h              # Configuration constants

data/                     # SPIFFS files
├── index.html           # Main control interface
├── settings.html        # Settings page
├── update.html          # OTA update page
├── style.css            # Styling
└── app.js               # Client-side JavaScript
```

Ready to begin implementation?