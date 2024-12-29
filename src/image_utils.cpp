// image_utils.cpp
#include "image_utils.h"
#include "display.h"
#include "config.h"
#include "esp_task_wdt.h"
#include <SPIFFS.h>
#include <Arduino.h> // For Serial.print

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

// File reading helper functions
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
    Serial.println("[IMAGE_UTILS] Starting BMP image processing...");
    fs::File file;
    bool valid = false; // valid format to be handled
    bool flip = true; // bitmap is stored bottom-to-top
    uint32_t startTime = millis();

    if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) {
        Serial.println("[IMAGE_UTILS] Image position out of display bounds");
        return;
    }

    Serial.printf("[IMAGE_UTILS] Opening file: /%s\n", filename);
    file = SPIFFS.open(String("/") + filename, "r");
    if (!file) {
        Serial.println("[IMAGE_UTILS] Failed to open file.");
        return;
    }

    // Parse BMP header
    if (read16(file) == 0x4D42) // BMP signature
    {
        uint32_t fileSize = read32(file);
        uint32_t creatorBytes = read32(file);
        (void) creatorBytes; // unused
        uint32_t imageOffset = read32(file); // Start of image data
        uint32_t headerSize = read32(file);
        uint32_t width = read32(file);
        int32_t height = (int32_t) read32(file);
        uint16_t planes = read16(file);
        uint16_t depth = read16(file); // bits per pixel
        uint32_t format = read32(file);

        Serial.println("[IMAGE_UTILS] BMP Header analysis:");
        Serial.printf("[IMAGE_UTILS] File Size: %u bytes\n", fileSize);
        Serial.printf("[IMAGE_UTILS] Image Offset: %u bytes\n", imageOffset);
        Serial.printf("[IMAGE_UTILS] Header Size: %u\n", headerSize);
        Serial.printf("[IMAGE_UTILS] Image Dimensions: %ux%u pixels\n", width, height);
        Serial.printf("[IMAGE_UTILS] Planes: %u\n", planes);
        Serial.printf("[IMAGE_UTILS] Bit Depth: %u\n", depth);
        Serial.printf("[IMAGE_UTILS] Format: %u\n", format);

        if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
        {
            // BMP rows are padded (if needed) to 4-byte boundary
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

            Serial.printf("[IMAGE_UTILS] Adjusted Image Size: %ux%u\n", w, h);

            if (w <= max_row_width) // handle with direct drawing
            {
                Serial.println("[IMAGE_UTILS] Image size within max_row_width. Proceeding with direct drawing.");

                // Reset WDT before long operation
                Serial.println("[IMAGE_UTILS] Resetting Watchdog Timer before image processing.");
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

                    Serial.println("[IMAGE_UTILS] Palette buffers populated.");
                }

                display.clearScreen();

                uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
                Serial.printf("[IMAGE_UTILS] Starting to process %u rows...\n", h);

                for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
                {
                    // Reset WDT in each iteration to prevent triggering
                    esp_task_wdt_reset();

                    uint32_t in_remain = rowSize;
                    uint32_t in_idx = 0;
                    uint32_t in_bytes = 0;
                    uint8_t in_byte = 0; // for depth <= 8
                    uint8_t in_bits = 0; // for depth <= 8
                    uint8_t out_byte = 0xFF; // white (for w%8!=0 border)
                    uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
                    uint32_t out_idx = 0;

                    file.seek(rowPosition);

                    for (uint16_t col = 0; col < w; col++) // for each pixel
                    {
                        // Time to read more pixel data?
                        if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
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
                                in_idx++; // skip alpha
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
                                if (format == 0) // 555
                                {
                                    blue = (lsb & 0x1F) << 3;
                                    green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                                    red = (msb & 0x7C) << 1;
                                } else // 565
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
                            // keep white
                        } else if (colored && with_color) {
                            out_color_byte &= ~(0x80 >> (col % 8)); // colored
                        } else {
                            out_byte &= ~(0x80 >> (col % 8)); // black
                        }

                        if ((7 == col % 8) || (col == w - 1)) // write that last byte! (for w%8!=0 border)
                        {
                            output_row_color_buffer[out_idx] = out_color_byte;
                            output_row_mono_buffer[out_idx++] = out_byte;
                            out_byte = 0xFF; // white (for w%8!=0 border)
                            out_color_byte = 0xFF; // white (for w%8!=0 border)
                        }
                    } // end pixel

                    uint16_t yrow = y + (flip ? h - row - 1 : row);
                    display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);

                    // Reset WDT after processing the row
                    esp_task_wdt_reset();
                } // end line

                Serial.printf("[IMAGE_UTILS] Image loaded in %lu ms\n", millis() - startTime);
                display.refresh();
                Serial.println("[IMAGE_UTILS] Display refreshed.");
            }
        }
    }
    file.close();
}

void drawProgmemFileFromSpiffs(const char *filename, uint16_t width, uint16_t height) {
    Serial.printf("[IMAGE_UTILS] Loading image file: %s\n", filename);

    bool with_color = true;

    fs::File file = SPIFFS.open(String("/") + filename, "r");
    if (!file) {
        Serial.println("[IMAGE_UTILS] Error: File access failed");
        return;
    }

    // One row requires width * 2 bytes (RGB565 = 2 bytes per pixel)
    const size_t rowSize = width * sizeof(uint16_t);
    uint8_t *rowBuffer = (uint8_t *) malloc(rowSize);
    if (!rowBuffer) {
        Serial.println("[IMAGE_UTILS] Failed to allocate memory for row buffer.");
        file.close();
        return;
    }

    // Two buffers of width/8 bytes each: monochrome and color
    uint8_t *monoBuffer = (uint8_t *) malloc(width / 8);
    uint8_t *colorBuffer = (uint8_t *) malloc(width / 8);

    if (!monoBuffer || !colorBuffer) {
        Serial.println("[IMAGE_UTILS] Failed to allocate memory for mono/color buffers.");
        free(rowBuffer);
        if (monoBuffer) free(monoBuffer);
        if (colorBuffer) free(colorBuffer);
        file.close();
        return;
    }

    Serial.printf("[IMAGE_UTILS] Starting to process binary file with dimensions %ux%u (RGB565)...\n", width, height);

    display.clearScreen();

    uint16_t y = 0;
    bool firstRowDebug = true; // for debugging first pixels

    while (y < height) {
        size_t bytesRead = file.read(rowBuffer, rowSize);
        if (bytesRead != rowSize) {
            Serial.printf("[IMAGE_UTILS] Incomplete row read at line %u: read %u bytes, expected %u.\n",
                          y, bytesRead, rowSize);
            break;
        }

        // Initially assume everything is white (bit=1)
        memset(monoBuffer, 0xFF, width / 8);
        memset(colorBuffer, 0xFF, width / 8);

        for (uint16_t col = 0; col < width; col++) {
            uint8_t lsb = rowBuffer[2 * col];
            uint8_t msb = rowBuffer[2 * col + 1];
            // Combine 16-bit
            uint16_t pixel565 = ((uint16_t) msb << 8) | lsb;

            // Extract RGB components
            uint8_t r = (pixel565 & 0xF800) >> 8; // 0..248 step 8
            uint8_t g = (pixel565 & 0x07E0) >> 3; // 0..252 step 4
            uint8_t b = (pixel565 & 0x001F) << 3; // 0..248 step 8

            // Determine if pixel is white/black or colored
            // Can adjust thresholds for better red/yellow separation
            bool whitish = ((uint16_t) r + (uint16_t) g + (uint16_t) b) > (3 * 128);
            bool colored = (r > 0xF0) || ((g > 0xF0) && (b > 0xF0));

            // Log first 10 pixels of the first row
            if (firstRowDebug && col < 10 && y == 0) {
                Serial.printf("[DEBUG] Pixel[%3u,%u]: R=%3u G=%3u B=%3u => w=%d c=%d\n",
                              col, y, r, g, b, whitish, colored);
            }

            if (!whitish) {
                // Pixel is either black or colored
                if (with_color && colored) {
                    // This is a color pixel
                    // monoBuffer - keep 1 (untouched => white)
                    // colorBuffer - clear bit => 0 = color
                    colorBuffer[col / 8] &= ~(0x80 >> (col & 7));
                } else {
                    // Consider it black
                    // monoBuffer => clear bit => 0 = black
                    // colorBuffer => remains 1 (white)
                    monoBuffer[col / 8] &= ~(0x80 >> (col & 7));
                }
            }
        }

        if (firstRowDebug && y == 0) {
            firstRowDebug = false;
            Serial.println("[DEBUG] --- End of row 0 debug output ---");
        }

        // For visual progress
        if (y % 32 == 0) {
            Serial.printf("[DEBUG] Converting row %u/%u ...\n", y, height);
        }

        // Call render function for the row
        display.writeImage(monoBuffer, colorBuffer, 0, y, width, 1);

        esp_task_wdt_reset();
        y++;
    }

    Serial.println("[IMAGE_UTILS] Display refresh complete.");
    display.refresh();
    Serial.printf("[IMAGE_UTILS] Processed %u lines in total.\n", y);

    free(colorBuffer);
    free(monoBuffer);
    free(rowBuffer);
    file.close();

    Serial.println("[IMAGE_UTILS] Image successfully displayed.");
}

// Task implementation for image rendering
void imageRenderTask(void *parameter) {
    Serial.println("[TASK] Image Render Task Started.");

    // Initialize WDT for CPU 1
    if (esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true) != ESP_OK) {
        Serial.println("[TASK] Failed to initialize Watchdog Timer on CPU 1.");
    } else {
        Serial.printf("[TASK] Watchdog Timer initialized on CPU 1 with %d seconds timeout.\n", WDT_TIMEOUT_SECONDS);
    }

    // Add current task to WDT monitoring
    if (esp_task_wdt_add(NULL) != ESP_OK) {
        Serial.println("[TASK] Failed to add Image Render Task to Watchdog Timer.");
    } else {
        Serial.println("[TASK] Image Render Task added to Watchdog Timer.");
    }

    // Draw the image
    drawProgmemFileFromSpiffs(SELECTED_IMAGE_BUFFER_PATH, 640, 384);

    Serial.println("[TASK] Image Render Task Completed.");

    // Remove task from WDT monitoring
    if (esp_task_wdt_delete(NULL) != ESP_OK) {
        Serial.println("[TASK] Failed to remove Image Render Task from Watchdog Timer.");
    } else {
        Serial.println("[TASK] Image Render Task removed from Watchdog Timer.");
    }

    // Task cleanup
    vTaskDelete(NULL);
}
