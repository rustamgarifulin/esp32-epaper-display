#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "filesystem.h"
#include "webserver.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include "esp_task_wdt.h"

unsigned long previousMillisLed = 0;
const long ledInterval = 500;
bool ledState = false;

unsigned long previousMillis = 0;
const long interval = 1000;

String data;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("[SYSTEM] Starting system initialization...");
    while (!Serial) {
        delay(100);
    }
    Serial.println("[SYSTEM] Serial communication established");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    Serial.println("[SYSTEM] LED pin initialized");

    initConfig();

    Serial.println("[FILESYSTEM] Initializing SPIFFS...");
    if (!SPIFFS.begin(true, "/data")) {
        Serial.println("[FILESYSTEM] ERROR: SPIFFS mount failed");
        return;
    }
    Serial.println("[FILESYSTEM] SPIFFS mounted successfully");

    File file = SPIFFS.open("/intro.txt");
    if (!file) {
        Serial.println("Failed to open /intro.txt");
    } else {
        data = file.readString();
        file.close();
        Serial.println("Content of /intro.txt:");
        Serial.println(data);
    }

    Serial.println("[DISPLAY] Initializing display hardware...");
    hspi.begin(13, 12, 14, 15);
    display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200);
    Serial.println("[DISPLAY] Display hardware initialized successfully");

    if (esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true) != ESP_OK) {
        Serial.println("[WDT] ERROR: Watchdog initialization failed");
    }
    Serial.println("[WDT] Watchdog timer initialized");

    Serial.println("[WIFI] Initiating connection...");
    Serial.printf("[WIFI] Connecting to SSID: %s\n", ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset();
    }

    Serial.println("\n[WIFI] Connection established");
    Serial.printf("[WIFI] IP address: %s\n", WiFi.localIP().toString().c_str());

    startWebserver();
    listFiles();
    Serial.println("[SYSTEM] System initialization complete");
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillisLed >= ledInterval) {
        previousMillisLed = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }

    handleDisplayJob();
    esp_task_wdt_reset();
}