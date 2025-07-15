#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "filesystem.h"
#include "webserver.h"
#include <WiFi.h>
#include <SPIFFS.h>
#include "esp_task_wdt.h"
#include <esp_sleep.h>
#include <HTTPClient.h>
#include "debug.h"

unsigned long lastFetchTime = 0;
bool shouldSleep = false;

unsigned long previousMillisLed = 0;
const long ledInterval = 500;
bool ledState = false;

unsigned long previousMillis = 0;
const long interval = 1000;

String data;

void fetchAndUpdateImage() {
    HTTPClient http;
    http.begin("http://192.168.88.209:3456/api/weather/image");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        File file = SPIFFS.open("/" SELECTED_IMAGE_BUFFER_PATH, "w");
        if (file) {
            http.writeToStream(&file);
            file.close();
            isImageRefreshPending = true;
            lastFetchTime = millis();
            shouldSleep = true;
        }
    }
    http.end();
}

void goToSleep() {
    debug.println("Going to sleep...");
    esp_sleep_enable_timer_wakeup(FETCH_INTERVAL * 1000);
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    // I2C-OLED display
    debug.begin();
    delay(100); // Даем время на стабилизацию I2C
    debug.println("[SYSTEM] Starting system initialization...");

    while (!Serial) {
        delay(100);
    }
    debug.println("[SYSTEM] Serial communication established");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    debug.println("[SYSTEM] LED pin initialized");

    initConfig();

    debug.println("[FILESYSTEM] Initializing SPIFFS...");
    if (!SPIFFS.begin(true, "/data")) {
        debug.println("[FILESYSTEM] ERROR: SPIFFS mount failed");
        return;
    }
    debug.println("[FILESYSTEM] SPIFFS mounted successfully");

    File file = SPIFFS.open("/intro.txt");
    if (!file) {
        debug.println("Failed to open /intro.txt");
    } else {
        data = file.readString();
        file.close();
        debug.println("Content of /intro.txt:");
        debug.println(data);
    }

    // E-Paper display
    debug.println("[DISPLAY] Initializing display hardware...");
    hspi.begin(13, 12, 14, 15);
    display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(115200);
    debug.println("[DISPLAY] Display hardware initialized successfully");

    if (esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true) != ESP_OK) {
        debug.println("[WDT] ERROR: Watchdog initialization failed");
    }
    debug.println("[WDT] Watchdog timer initialized");

    debug.println("[WIFI] Initiating connection...");
    debug.println("[WIFI] Connecting to SSID: " + String(ssid));
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        esp_task_wdt_reset();
    }

    debug.println("\n[WIFI] Connection established");
    debug.println("[WIFI] IP address: " + String(WiFi.localIP().toString().c_str()));

    startWebserver();
    listFiles();
    debug.println("[SYSTEM] System initialization complete");
}

void loop() {
    unsigned long currentMillis = millis();

    // LED-indication
    if (currentMillis - previousMillisLed >= ledInterval) {
        previousMillisLed = currentMillis;
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }

    // Check image after wakeup
    if (lastFetchTime == 0) {
        fetchAndUpdateImage();
    }

    handleDisplayJob();

    debug.println("[SYSTEM] Idle");

    // And going to sleep
    if (shouldSleep && !isImageRefreshPending && !isDisplayJobScheduled &&
        (currentMillis - lastFetchTime > DISPLAY_DELAY)) {
        goToSleep(); // Sleep for FETCH_INTERVAL
    }

    esp_task_wdt_reset();
}
