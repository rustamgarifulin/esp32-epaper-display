#include "debug.h"

Debug debug;

Debug::Debug() {
    display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
    currentMessage = "";
}

void Debug::begin() {
    Wire.begin(I2C_SDA, I2C_SCL, 100000);
    delay(100);

    Wire.beginTransmission(SCREEN_ADDRESS);
    if (Wire.endTransmission() != 0) {
        Serial.println(F("SSD1306 not found at 0x3C"));
        return;
    }

    if(!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        return;
    }

    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    display->display();
}

void Debug::updateDisplay() {
    display->clearDisplay();
    display->setCursor(0, 0);

    String message = currentMessage;
    int charWidth = 6; // approximately
    int charsPerLine = SCREEN_WIDTH / charWidth;

    while (message.length() > 0) {
        String line;
        if (message.length() > charsPerLine) {
            line = message.substring(0, charsPerLine);
            message = message.substring(charsPerLine);
        } else {
            line = message;
            message = "";
        }
        display->println(line);
    }

    display->display();
}

void Debug::println(const String &message) {
    static bool isProcessing = false;

    if (isProcessing) {
        Serial.println(message);
        return;
    }

    isProcessing = true;

    Serial.println(message);

    if (display != nullptr) {
        currentMessage = message;
        updateDisplay();
    }

    isProcessing = false;
}

void Debug::println(const char* message) {
    println(String(message));
}

void Debug::println(char message) {
    println(String(message));
}

void Debug::println(int message) {
    println(String(message));
}

void Debug::println(long message) {
    println(String(message));
}

void Debug::println(double message, int precision) {
    String str = String(message, precision);
    println(str);
}
