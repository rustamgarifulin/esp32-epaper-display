// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

extern const char *hostname;
extern const char *ssid;
extern const char *password;

extern int16_t DISPLAY_X;
extern int16_t DISPLAY_Y;
extern int16_t DISPLAY_WIDTH;
extern int16_t DISPLAY_HEIGHT;

#define SELECTED_IMAGE_BUFFER_PATH "image.bin"

extern const int WDT_TIMEOUT_SECONDS;

extern const int LED_PIN;

void initConfig();

#endif
