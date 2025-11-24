#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define I2C_SDA 21
#define I2C_SCL 22

class Debug {
private:
    String currentMessage;
    Adafruit_SSD1306* display;

    void scrollLines();
    void updateDisplay();

public:
    Debug();
    void begin();
    void println(const String &message);
    void println(const char* message);
    void println(char message);
    void println(int message);
    void println(long message);
    void println(double message, int precision = 2);
};

extern Debug debug;

#endif