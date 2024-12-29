#ifndef DISPLAY_H
#define DISPLAY_H

#include <ESPAsyncWebServer.h>
#include <GxEPD2_3C.h>
#include <SPI.h>
#include "config.h"

extern GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display;
extern SPIClass hspi;

extern bool isImageRefreshPending;
extern bool isDisplayJobScheduled;
extern unsigned long displayJobStart;

void clearDisplay();

void showSelectedImage();

void handleDisplayJob();

#endif
