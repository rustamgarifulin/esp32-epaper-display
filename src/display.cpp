// display.cpp
#include "display.h"
#include "image_utils.h"
#include "filesystem.h"
#include "esp_task_wdt.h"
#include "debug.h"

#include <Arduino.h>

GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display(GxEPD2_750c(/*CS=*/15, /*DC=*/27, /*RST=*/26, /*BUSY=*/25));
SPIClass hspi(HSPI);

bool isImageRefreshPending = false;
bool isDisplayJobScheduled = false;
unsigned long displayJobStart = 0;

void clearDisplay() {
    debug.println("[DISPLAY] Initiating display clear operation");
    debug.println("[DISPLAY] Clearing Display...");
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());
    // display.clearScreen();
    debug.println("[DISPLAY] Display Cleared.");
    debug.println("[DISPLAY] Display clear operation completed");
}

void showSelectedImage() {
    debug.println("[DISPLAY] Starting image rendering process");
        clearDisplay();
        debug.println("[DISPLAY] Showing Selected Image...");
    drawProgmemFileFromSpiffs(SELECTED_IMAGE_BUFFER_PATH, 640, 384);
    debug.println("[DISPLAY] Selected Image Displayed.");
    debug.println("[DISPLAY] Image rendering completed");
}

void handleDisplayJob() {
    if (isImageRefreshPending) {
        debug.println("[DISPLAY] Scheduling display refresh job");
        debug.println("[DISPLAY] Image refresh pending. Scheduling display job...");
        displayJobStart = millis() + 1000UL;
        isImageRefreshPending = false;
        isDisplayJobScheduled = true;
        debug.println("[DISPLAY] Job scheduled for timestamp: " + String(displayJobStart) + "ms");
    }
    if (isDisplayJobScheduled && millis() > displayJobStart) {
        debug.println("[DISPLAY] Executing scheduled display job...");
        isDisplayJobScheduled = false;
        showSelectedImage();
        debug.println("[DISPLAY] Display job completed.");
    }
}
