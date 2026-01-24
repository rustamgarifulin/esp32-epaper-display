#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "filesystem.h"
#include "webserver.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include "esp_task_wdt.h"
#include "debug.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

unsigned long previousMillisLed = 0;
const long ledInterval = 500;  // LED blink interval
bool ledState = false;

unsigned long previousMillis = 0;
const long interval = 1000;

unsigned long previousStatusUpdate = 0;
const long statusUpdateInterval = 5000;  // Update status message every 5 seconds

String data;

void setup() {
    // CRITICAL: Disable brownout detector first
    // This prevents reboot on voltage drops from USB cable
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n=== ESP32 WEATHER STATION ===");
    Serial.println("Starting initialization...");
    Serial.println("Brownout detector DISABLED for USB power");

    // Early WiFi mode initialization for stability
    WiFi.mode(WIFI_STA);
    delay(100);

    // I2C-OLED display
    Serial.println("Initializing OLED debug...");
    debug.begin();
    delay(200); // Increased delay for I2C stabilization
    Serial.println("OLED initialized");
    debug.println("[SYSTEM] Starting system initialization...");

    while (!Serial) {
        delay(100);
    }
    debug.println("[SYSTEM] Serial communication established");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    debug.println("[SYSTEM] LED pin initialized");
    delay(200); // Delay before next module

    Serial.println("Initializing SPIFFS...");
    debug.println("[FILESYSTEM] Initializing SPIFFS...");

    // Try to format if mount fails
    if (!SPIFFS.begin(true, "/data")) {
        Serial.println("SPIFFS mount failed! Attempting format...");
        debug.println("[FILESYSTEM] ERROR: SPIFFS mount failed, formatting...");

        SPIFFS.format();
        delay(1000);

        if (!SPIFFS.begin(true, "/data")) {
            Serial.println("SPIFFS format failed! System halted.");
            debug.println("[FILESYSTEM] CRITICAL: Cannot initialize SPIFFS!");
            while(1) {
                digitalWrite(LED_PIN, HIGH);
                delay(100);
                digitalWrite(LED_PIN, LOW);
                delay(100);
            }
        }
    }
    Serial.println("SPIFFS mounted successfully");
    debug.println("[FILESYSTEM] SPIFFS mounted successfully");

    File file = SPIFFS.open("/data/intro.txt");
    if (!file) {
        debug.println("Failed to open /data/intro.txt");
    } else {
        data = file.readString();
        file.close();
        debug.println("Content of /data/intro.txt:");
        debug.println(data);
    }

    if (esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true) != ESP_OK) {
        debug.println("[WDT] ERROR: Watchdog initialization failed");
    }
    debug.println("[WDT] Watchdog timer initialized");
    delay(200); // Delay after WDT

    // WiFi connection BEFORE display initialization (to spread power consumption peaks)
    debug.println("[WIFI] Initiating connection...");

    // Increased delay before WiFi for power stabilization
    delay(1000); // Was 500ms, increased to 1000ms

    String wifiMsg = "[WIFI] Connecting to SSID: ";
    wifiMsg += ssid;
    debug.println(wifiMsg);

    // Set minimum transmitter power to reduce consumption
    WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Minimum power for USB
    debug.println("[WIFI] TX power set to 8.5dBm (minimum for USB)");

    // Disable WiFi sleep mode for stability
    WiFi.setSleep(false);

    WiFi.begin(ssid, password);

    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 40) {
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset();
        wifiAttempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        debug.println("\n[WIFI] ERROR: Failed to connect after 20 seconds");
        debug.println("[WIFI] Restarting ESP32...");
        delay(1000);
        ESP.restart();
    }

    debug.println("\n[WIFI] Connection established");
    String ipMsg = "[WIFI] IP address: ";
    ipMsg += WiFi.localIP().toString();
    debug.println(ipMsg);

    // Delay after WiFi before display initialization
    debug.println("[SYSTEM] Waiting before display init...");
    delay(1000);

    // E-Paper display (after WiFi)
    debug.println("[DISPLAY] Initializing display hardware...");
    hspi.begin(13, 12, 14, 15);
    display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200);
    debug.println("[DISPLAY] Display hardware initialized successfully");

    startWebserver();
    listFiles();
    debug.println("[SYSTEM] System initialization complete");
    debug.println("[SYSTEM] Running on USB power");
}

void loop() {
    unsigned long currentMillis = millis();

    // LED indication
    if (currentMillis - previousMillisLed >= ledInterval) {
        previousMillisLed = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }

    // Handle display updates when triggered by server
    handleDisplayJob();

    // Update status message every 5 seconds
    if (currentMillis - previousStatusUpdate >= statusUpdateInterval) {
        previousStatusUpdate = currentMillis;
        String statusMsg = "[SYSTEM] Ready | IP: ";
        statusMsg += WiFi.localIP().toString();
        debug.println(statusMsg);
    }

    esp_task_wdt_reset();
}
