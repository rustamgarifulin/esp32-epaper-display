# ESP32 E-Paper Display Controller

A versatile ESP32-based controller for e-paper displays with web interface support. This project allows you to upload and display images on e-paper displays through a web interface.

## Related Projects

- **[Weather Server](https://github.com/rustamgarifulin/esp32-epaper-display-server)** - Bun server that generates weather images and sends them to this device

---

I’m not a C++ developer and don’t understand many concepts of this language. I’ll be happy to receive pull requests and feature requests in the Issues section.

---

## Features

- 🖼️ Support for e-paper displays (specifically GxEPD2_750c)
- 🌐 Web interface for image upload and control
- 📡 Wi-Fi connectivity
- 💾 SPIFFS file system for image storage
- 🎨 Support for both monochrome and color images
- 🔄 Async web server for better performance
- 📊 System monitoring endpoints
- 💩 Not all features work as intended. The code mostly works for my use cases (e.g., a pet weather station project). The code likely contains bugs, but I’ve tried to make it as readable as possible.

## Hardware Requirements

- ESP32 development board
- Waveshare 7.5" three-color e-paper display
- Power supply (USB or external)

## Pin Configuration

```
Display connections:
- BUSY -> GPIO25
- RST  -> GPIO26
- DC   -> GPIO27
- CS   -> GPIO15
- CLK  -> GPIO13
- DIN  -> GPIO14
```

## Software Setup

1. Install PlatformIO IDE (VSCode extension)
2. Clone this repository
3. Update WiFi credentials in `src/config.cpp`
4. Build and upload the project:

```bash
# Using PlatformIO CLI
pio run -t upload
# Monitor serial output
pio device monitor
```

## Web API Endpoints

- `GET /` - Hello world test
- `GET /api/system/memory` - System memory usage
- `GET /api/system/list` - List files in SPIFFS
- `GET /api/image/draw` - Trigger display refresh
- `POST /api/image/upload` - Upload new image
- `GET /fs/*` - Static file server (SPIFFS)

## Usage Examples

### Upload Image via cURL

```bash
# Upload an image
curl -X POST -F "file=@image.bin" http://esp32-ip/api/image/upload

# Trigger display refresh
curl http://esp32-ip/api/image/draw

# Check system memory
curl http://esp32-ip/api/system/memory
```

### Image Format Requirements

- Format: BMP or raw RGB565
- Resolution: 640x384 pixels
- Color: Three-color (black, white, red/yellow)

## Development

### Project Structure

```
main/
├── src/
│   ├── main.cpp          # Main application entry
│   ├── webserver.cpp     # Web server implementation
│   ├── display.cpp       # Display controller
│   ├── image_utils.cpp   # Image processing utilities
│   ├── filesystem.cpp    # SPIFFS operations
│   └── config.cpp        # Configuration
├── include/
│   └── *.h              # Header files
└── platformio.ini        # PlatformIO configuration
```

### Building from Source

1. Install dependencies:
```ini
lib_deps =
    ESP Async WebServer
    AsyncTCP
    ArduinoJson
    Adafruit GFX Library
    GxEPD2
    Wire
```

2. Configure your environment:
```bash
# Edit WiFi settings
nano src/config.cpp
```

3. Build and upload:
```bash
pio run -t upload
```

## Troubleshooting

- **Display not updating**: Check SPI connections and reset the device.
- **Upload fails**: Verify WiFi connection and file size (max 3MB).
- **Memory errors**: Reduce image size or clear SPIFFS.

## License

MIT License
