#include "config.h"
#include "debug.h"

#include <Arduino.h>

const char *hostname = "esp32";
const char *ssid = "EGOR";
const char *password = "ohmyglob";

int16_t DISPLAY_WIDTH = 640;
int16_t DISPLAY_HEIGHT = 384;

const int WDT_TIMEOUT_SECONDS = 120;

const int LED_PIN = 2;

void initConfig() {
    debug.println("[CONFIG] Starting configuration initialization...");
    debug.println("[CONFIG] Setting hostname: " + String(hostname));
    debug.println("[CONFIG] Configuring WiFi - SSID: " + String(ssid));
    debug.println("[CONFIG] Display dimensions: " + String(DISPLAY_WIDTH) + String(DISPLAY_HEIGHT));
    debug.println("[CONFIG] Watchdog timeout: " + String(WDT_TIMEOUT_SECONDS) + " seconds");
    debug.println("[CONFIG] LED pin configured: " + String(LED_PIN));
    debug.println("[CONFIG] Configuration initialization completed");
}
