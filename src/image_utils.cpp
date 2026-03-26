// image_utils.cpp
#include "image_utils.h"
#include "display.h"
#include "config.h"
#include "esp_task_wdt.h"
#include <LittleFS.h>
#include <Arduino.h>
#include "debug.h"

// Buffer variables for image processing
static const uint16_t input_buffer_pixels = 800;
static const uint16_t max_row_width = 640;
static const uint16_t max_palette_pixels = 256;

uint8_t input_buffer[3 * input_buffer_pixels];
uint8_t output_row_mono_buffer[max_row_width / 8];
uint8_t output_row_color_buffer[max_row_width / 8];
uint8_t mono_palette_buffer[max_palette_pixels / 8];
uint8_t color_palette_buffer[max_palette_pixels / 8];
uint16_t rgb_palette_buffer[max_palette_pixels];

uint16_t read16(fs::File &f) {
    uint16_t result;
    ((uint8_t *) &result)[0] = f.read();
    ((uint8_t *) &result)[1] = f.read();
    return result;
}

uint32_t read32(fs::File &f) {
    uint32_t result;
    ((uint8_t *) &result)[0] = f.read();
    ((uint8_t *) &result)[1] = f.read();
    ((uint8_t *) &result)[2] = f.read();
    ((uint8_t *) &result)[3] = f.read();
    return result;
}

void drawBitmapFromSpiffs(const char *filename, int16_t x, int16_t y, bool with_color) {
    debug.println("[IMAGE_UTILS] Starting BMP image processing...");
    fs::File file;
    bool valid = false;
    bool flip = true;
    uint32_t startTime = millis();

    if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) {
        debug.println("[IMAGE_UTILS] Image position out of display bounds");
        return;
    }

    debug.println("[IMAGE_UTILS] Opening file: " + String(filename));
    // LittleFS is mounted at /data
    file = LittleFS.open(String("/") + filename, "r");
    if (!file) {
        debug.println("[IMAGE_UTILS] Failed to open file at path: /data/" + String(filename));
        return;
    }

    // Parse BMP header
    if (read16(file) == 0x4D42)
    {
        uint32_t fileSize = read32(file);
        uint32_t creatorBytes = read32(file);
        (void) creatorBytes;
        uint32_t imageOffset = read32(file);
        uint32_t headerSize = read32(file);
        uint32_t width = read32(file);
        int32_t height = (int32_t) read32(file);
        uint16_t planes = read16(file);
        uint16_t depth = read16(file);
        uint32_t format = read32(file);

        debug.println("[IMAGE_UTILS] BMP Header analysis:");
        debug.println("[IMAGE_UTILS] File Size: " + String(fileSize) + " bytes");
        debug.println("[IMAGE_UTILS] Image Offset: " + String(imageOffset) + " bytes");
        debug.println("[IMAGE_UTILS] Header Size: " + String(headerSize) + " bytes");
        debug.println("[IMAGE_UTILS] Image Dimensions: " + String(width) + "x" + String(height));
        debug.println("[IMAGE_UTILS] Planes: " + String(planes));
        debug.println("[IMAGE_UTILS] Bit Depth: " + String(depth));
        debug.println("[IMAGE_UTILS] Format: " + String(format));

        if ((planes == 1) && ((format == 0) || (format == 3)))
        {

            uint32_t rowSize = (width * depth / 8 + 3) & ~3;
            if (depth < 8)
                rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
            if (height < 0) {
                height = -height;
                flip = false;
            }

            uint16_t w = width;
            uint16_t h = height;

            if ((x + w - 1) >= display.epd2.WIDTH)
                w = display.epd2.WIDTH - x;
            if ((y + h - 1) >= display.epd2.HEIGHT)
                h = display.epd2.HEIGHT - y;

            debug.println("[IMAGE_UTILS] Adjusted Image Size: " + String(w) + "x" + String(h));

            if (w <= max_row_width)
            {
                debug.println("[IMAGE_UTILS] Image size within max_row_width. Proceeding with direct drawing.");


                debug.println("[IMAGE_UTILS] Resetting Watchdog Timer before image processing.");
                esp_task_wdt_reset();

                valid = true;
                uint8_t bitmask = 0xFF;
                uint8_t bitshift = 8 - depth;
                uint16_t red, green, blue;
                bool whitish = false;
                bool colored = false;

                if (depth == 1)
                    with_color = false;

                if (depth <= 8) {
                    if (depth < 8)
                        bitmask >>= depth;
                    file.seek(imageOffset - (4 << depth)); // 54 for regular, diff for colors important

                    for (uint16_t pn = 0; pn < (1 << depth); pn++) {
                        blue = file.read();
                        green = file.read();
                        red = file.read();
                        file.read(); // skip alpha

                        whitish = with_color
                                      ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                      : ((red + green + blue) > 3 * 0x80);
                        colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));

                        if (0 == pn % 8)
                            mono_palette_buffer[pn / 8] = 0;
                        mono_palette_buffer[pn / 8] |= whitish << (pn % 8);

                        if (0 == pn % 8)
                            color_palette_buffer[pn / 8] = 0;
                        color_palette_buffer[pn / 8] |= colored << (pn % 8);
                    }

                    debug.println("[IMAGE_UTILS] Palette buffers populated.");
                }

                display.clearScreen();

                uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                debug.println("[IMAGE_UTILS] Starting to process " + String(h) + " rows...");

                for (uint16_t row = 0; row < h; row++, rowPosition += rowSize)
                {

                    esp_task_wdt_reset();

                    uint32_t in_remain = rowSize;
                    uint32_t in_idx = 0;
                    uint32_t in_bytes = 0;
                    uint8_t in_byte = 0;
                    uint8_t in_bits = 0;
                    uint8_t out_byte = 0xFF;
                    uint8_t out_color_byte = 0xFF;
                    uint32_t out_idx = 0;

                    file.seek(rowPosition);

                    for (uint16_t col = 0; col < w; col++)
                    {

                        if (in_idx >= in_bytes)
                        {
                            in_bytes = file.read(input_buffer,
                                                 (in_remain > sizeof(input_buffer)) ? sizeof(input_buffer) : in_remain);
                            in_remain -= in_bytes;
                            in_idx = 0;
                        }

                        switch (depth) {
                            case 32:
                                blue = input_buffer[in_idx++];
                                green = input_buffer[in_idx++];
                                red = input_buffer[in_idx++];
                                in_idx++;
                                whitish = with_color
                                              ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                              : ((red + green + blue) > 3 * 0x80);
                                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
                                break;
                            case 24:
                                blue = input_buffer[in_idx++];
                                green = input_buffer[in_idx++];
                                red = input_buffer[in_idx++];
                                whitish = with_color
                                              ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                              : ((red + green + blue) > 3 * 0x80);
                                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
                                break;
                            case 16: {
                                uint8_t lsb = input_buffer[in_idx++];
                                uint8_t msb = input_buffer[in_idx++];
                                if (format == 0)
                                {
                                    blue = (lsb & 0x1F) << 3;
                                    green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                    red = (msb & 0x7C) << 1;
                                } else
                                {
                                    blue = (lsb & 0x1F) << 3;
                                    green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                                    red = (msb & 0xF8);
                                }
                                whitish = with_color
                                              ? ((red > 0x80) && (green > 0x80) && (blue > 0x80))
                                              : ((red + green + blue) > 3 * 0x80);
                                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
                            }
                            break;
                            case 1:
                            case 2:
                            case 4:
                            case 8: {
                                if (0 == in_bits) {
                                    in_byte = input_buffer[in_idx++];
                                    in_bits = 8;
                                }
                                uint16_t pn = (in_byte >> bitshift) & bitmask;
                                whitish = mono_palette_buffer[pn / 8] & (0x1 << (pn % 8));
                                colored = color_palette_buffer[pn / 8] & (0x1 << (pn % 8));
                                in_byte <<= depth;
                                in_bits -= depth;
                            }
                            break;
                        }

                        if (whitish) {

                        } else if (colored && with_color) {
                            out_color_byte &= ~(0x80 >> (col % 8));
                        } else {
                            out_byte &= ~(0x80 >> (col % 8));
                        }

                        if ((7 == col % 8) || (col == w - 1))
                        {
                            output_row_color_buffer[out_idx] = out_color_byte;
                            output_row_mono_buffer[out_idx++] = out_byte;
                            out_byte = 0xFF;
                            out_color_byte = 0xFF;
                        }
                    }

                    uint16_t yrow = y + (flip ? h - row - 1 : row);
                    display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);


                    esp_task_wdt_reset();
                }

                debug.println("[IMAGE_UTILS] Image loaded in " + String(millis() - startTime) + " ms");
                display.refresh();
                debug.println("[IMAGE_UTILS] Display refreshed.");
            }
        }
    }
    file.close();
}

void drawProgmemFileFromSpiffs(const char *filename, uint16_t width, uint16_t height) {
    Serial.println("[IMAGE_UTILS] >>> drawProgmemFileFromSpiffs START");
    Serial.flush();
    unsigned long totalStart = millis();

    bool with_color = true;

    String filePath = String("/") + filename;
    fs::File file = LittleFS.open(filePath, "r");
    if (!file) {
        debug.println("[IMAGE_UTILS] Error: File access failed at path: " + filePath);
        return;
    }

    size_t fileSize = file.size();
    size_t expectedSize = width * height * sizeof(uint16_t);
    Serial.println("[IMAGE_UTILS] File size: " + String(fileSize) + " expected: " + String(expectedSize));
    Serial.flush();

    if (fileSize != expectedSize) {
        Serial.println("[IMAGE_UTILS] ERROR: File size mismatch!");
        file.close();
        return;
    }

    // Batch processing: BATCH_ROWS rows at a time to reduce SPI command overhead
    const uint16_t BATCH_ROWS = 16;
    const size_t rowBytes = width / 8;  // 80 bytes per row for mono/color
    const size_t rowSize = width * sizeof(uint16_t);  // 1280 bytes per row RGB565

    // Allocate batch buffers
    uint8_t *readBuffer = (uint8_t *) malloc(rowSize * BATCH_ROWS);
    uint8_t *monoBuffer = (uint8_t *) malloc(rowBytes * BATCH_ROWS);
    uint8_t *colorBuffer = (uint8_t *) malloc(rowBytes * BATCH_ROWS);

    if (!readBuffer || !monoBuffer || !colorBuffer) {
        debug.println("[IMAGE_UTILS] Failed to allocate buffers");
        if (readBuffer) free(readBuffer);
        if (monoBuffer) free(monoBuffer);
        if (colorBuffer) free(colorBuffer);
        file.close();
        return;
    }

    Serial.println("[IMAGE_UTILS] Batch size: " + String(BATCH_ROWS) + " rows, buffer: " + String(rowSize * BATCH_ROWS) + " bytes");

    // Initialize display controller and RAM (no refresh). Only 289ms vs 32s for clearScreen().
    unsigned long t0 = millis();
    display.writeScreenBuffer();
    Serial.println("[TIMING] writeScreenBuffer: " + String(millis() - t0) + " ms");

    // Process image in batches
    t0 = millis();
    uint16_t y = 0;

    while (y < height) {
        uint16_t batchH = min((uint16_t)BATCH_ROWS, (uint16_t)(height - y));
        size_t batchReadSize = rowSize * batchH;

        // Read batch from LittleFS
        size_t bytesRead = file.read(readBuffer, batchReadSize);
        if (bytesRead != batchReadSize) {
            debug.println("[IMAGE_UTILS] Read error at row " + String(y));
            break;
        }

        // Initialize batch output buffers to white
        memset(monoBuffer, 0xFF, rowBytes * batchH);
        memset(colorBuffer, 0xFF, rowBytes * batchH);

        // Convert RGB565 to 1bpp mono + color
        for (uint16_t row = 0; row < batchH; row++) {
            uint8_t *rowData = readBuffer + row * rowSize;
            uint8_t *monoRow = monoBuffer + row * rowBytes;
            uint8_t *colorRow = colorBuffer + row * rowBytes;

            for (uint16_t col = 0; col < width; col++) {
                uint16_t idx = col * 2;
                uint16_t pixel565 = ((uint16_t)rowData[idx + 1] << 8) | rowData[idx];

                uint8_t r = (pixel565 & 0xF800) >> 8;
                uint8_t g = (pixel565 & 0x07E0) >> 3;
                uint8_t b = (pixel565 & 0x001F) << 3;

                bool whitish = ((uint16_t)r + g + b) > 384;

                if (!whitish) {
                    bool colored = (r > 0xF0) || ((g > 0xF0) && (b > 0xF0));
                    uint8_t mask = 0x80 >> (col & 7);
                    uint16_t byteIdx = col >> 3;

                    if (with_color && colored) {
                        colorRow[byteIdx] &= ~mask;
                    } else {
                        monoRow[byteIdx] &= ~mask;
                    }
                }
            }
        }

        // Write batch to display controller in one call
        display.writeImage(monoBuffer, colorBuffer, 0, y, width, batchH);

        esp_task_wdt_reset();
        y += batchH;

        if (y % 64 == 0 || y >= height) {
            Serial.println("[TIMING] Processed " + String(y) + "/" + String(height) + " rows");
        }
    }

    unsigned long processTime = millis() - t0;
    Serial.println("[TIMING] Read + convert + write: " + String(processTime) + " ms (" + String(y) + " rows)");

    // Single display refresh (hardware limit ~32s for 3-color e-paper)
    t0 = millis();
    Serial.println("[TIMING] Starting display.refresh()...");
    display.refresh();
    Serial.println("[TIMING] Display refresh: " + String(millis() - t0) + " ms");
    Serial.println("[TIMING] Total pipeline: " + String(millis() - totalStart) + " ms");

    free(colorBuffer);
    free(monoBuffer);
    free(readBuffer);
    file.close();
}

// Task implementation for image rendering
void imageRenderTask(void *parameter) {
    debug.println("[TASK] Image Render Task Started.");

    // Initialize WDT for CPU 1
    if (esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true) != ESP_OK) {
        debug.println("[TASK] Failed to initialize Watchdog Timer on CPU 1.");
    } else {
        debug.println("[TASK] Watchdog Timer initialized on CPU 1 with " + String(WDT_TIMEOUT_SECONDS) + " seconds timeout.");
    }

    // Add current task to WDT monitoring
    if (esp_task_wdt_add(NULL) != ESP_OK) {
        debug.println("[TASK] Failed to add Image Render Task to Watchdog Timer.");
    } else {
        debug.println("[TASK] Image Render Task added to Watchdog Timer.");
    }

    // Draw the image
    drawProgmemFileFromSpiffs(SELECTED_IMAGE_BUFFER_PATH, 640, 384);

    debug.println("[TASK] Image Render Task Completed.");

    // Remove task from WDT monitoring
    if (esp_task_wdt_delete(NULL) != ESP_OK) {
        debug.println("[TASK] Failed to remove Image Render Task from Watchdog Timer.");
    } else {
        debug.println("[TASK] Image Render Task removed from Watchdog Timer.");
    }

    // Task cleanup
    vTaskDelete(NULL);
}
