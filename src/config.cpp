#include "config.h"

#include <Arduino.h>

const char *hostname = "esp32";
const char *ssid = "EGOR";
const char *password = "ohmyglob";

int16_t DISPLAY_WIDTH = 640;
int16_t DISPLAY_HEIGHT = 384;

const int WDT_TIMEOUT_SECONDS = 120;

const int LED_PIN = 2;

void initConfig() {
    Serial.println("[CONFIG] Starting configuration initialization...");
    Serial.printf("[CONFIG] Setting hostname: %s\n", hostname);
    Serial.printf("[CONFIG] Configuring WiFi - SSID: %s\n", ssid);
    Serial.printf("[CONFIG] Display dimensions: %dx%d\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    Serial.printf("[CONFIG] Watchdog timeout: %d seconds\n", WDT_TIMEOUT_SECONDS);
    Serial.printf("[CONFIG] LED pin configured: %d\n", LED_PIN);
    Serial.println("[CONFIG] Configuration initialization completed");
}
