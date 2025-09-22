# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

- **Build project**: `~/.platformio/penv/bin/pio run`
- **Upload firmware**: `~/.platformio/penv/bin/pio run --target upload`
- **Monitor serial output**: `~/.platformio/penv/bin/pio monitor`
- **Clean build**: `~/.platformio/penv/bin/pio run --target clean`
- **Upload SPIFFS**: `~/.platformio/penv/bin/pio run --target uploadfs`

### Build Configurations
The project has three build configurations controlled by `build_src_filter` in platformio.ini:
- **Normal operation**: Excludes test_serial.cpp and clear_wifi.cpp
- **Test serial only**: Excludes main.cpp (uncomment to test RS485 communication)
- **Clear WiFi credentials**: Excludes main.cpp and test_serial.cpp (uncomment to clear stored WiFi)

## Architecture Overview

This ESP32-S3 project implements a web-based controller for Doosan G20 VFD (Variable Frequency Drive) using Modbus RTU over RS485.

### Core Components

1. **ModbusVFD** (src/ModbusVFD.cpp/h)
   - Handles RS485/Modbus RTU communication with G20 VFD
   - Implements frequency control, start/stop, and status monitoring
   - Uses hardware serial (Serial1) on pins: TX=17, RX=18, DE=21
   - Key registers defined in Config.h based on G20 documentation

2. **WiFiManager** (src/WiFiManager.cpp/h)
   - Dual-mode WiFi: AP mode for initial setup, STA mode for normal operation
   - Stores credentials in ESP32 Preferences (NVS)
   - AP mode SSID: "G20_Controller_Setup" with captive portal at 10.0.0.1
   - Automatic reconnection and mDNS support (g20-controller.local)

3. **WebInterface** (src/WebInterface.cpp/h)
   - Custom HTTP server (SimpleHTTPServer) adapted from AiO project
   - WebSocket server (SimpleWebSocket) for real-time updates on port 81
   - Serves static files from SPIFFS (data/ directory)
   - Handles VFD control commands and status updates

### Communication Flow
1. Web client → WebSocket command → WebInterface
2. WebInterface → ModbusVFD → RS485/Modbus RTU → G20 VFD
3. G20 VFD → ModbusVFD → WebInterface → WebSocket update → Web client

### Hardware Configuration
- **Board**: ESP32-S3-DevKitC-1 with USB CDC enabled
- **RS485**: Uses DE pin for direction control (half-duplex)
- **Modbus**: 9600 baud, 8N1, slave ID 1 (G20 default)

## Testing Approach
- Use `test_serial.cpp` to test RS485 communication independently
- Monitor serial output at 115200 baud for debug messages
- Test examples in `test/examples/` demonstrate WiFi AP/STA modes
- G20 documentation PDFs in `test/` provide register maps and protocol details

## Version Management
Version information is in `include/version.h`. When fixing bugs, increment VERSION_PATCH.