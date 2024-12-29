// display.cpp
#include "display.h"
#include "image_utils.h"
#include "filesystem.h"
#include "esp_task_wdt.h"

#include <Arduino.h>

GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display(GxEPD2_750c(/*CS=*/15, /*DC=*/27, /*RST=*/26, /*BUSY=*/25));
SPIClass hspi(HSPI);

bool isImageRefreshPending = false;
bool isDisplayJobScheduled = false;
unsigned long displayJobStart = 0;

void clearDisplay() {
    Serial.println("[DISPLAY] Initiating display clear operation");
    Serial.println("[DISPLAY] Clearing Display...");
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());
    Serial.println("[DISPLAY] Display Cleared.");
    Serial.println("[DISPLAY] Display clear operation completed");
}

void showSelectedImage() {
    Serial.println("[DISPLAY] Starting image rendering process");
    Serial.println("[DISPLAY] Showing Selected Image...");
    drawProgmemFileFromSpiffs(SELECTED_IMAGE_BUFFER_PATH, 640, 384);
    Serial.println("[DISPLAY] Selected Image Displayed.");
    Serial.println("[DISPLAY] Image rendering completed");
}

void handleDisplayJob() {
    if (isImageRefreshPending) {
        Serial.println("[DISPLAY] Scheduling display refresh job");
        Serial.println("[DISPLAY] Image refresh pending. Scheduling display job...");
        displayJobStart = millis() + 1000UL;
        isImageRefreshPending = false;
        isDisplayJobScheduled = true;
        Serial.printf("[DISPLAY] Job scheduled for timestamp: %lu ms\n", displayJobStart);
    }
    if (isDisplayJobScheduled && millis() > displayJobStart) {
        Serial.println("[DISPLAY] Executing scheduled display job...");
        isDisplayJobScheduled = false;
        showSelectedImage();
        Serial.println("[DISPLAY] Display job completed.");
    }
}
