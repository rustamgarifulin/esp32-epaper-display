#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <Arduino.h>
#include "FS.h"

uint16_t read16(fs::File &f);
uint32_t read32(fs::File &f);

/**
 * Displays a BMP image from SPIFFS storage
 * Supported color depths:
 * - 1 bit per pixel (monochrome)
 * - 2, 4, or 8 bits per pixel (palette colors)
 * - 16 bits per pixel (RGB565 or RGB555)
 * - 24 bits per pixel (RGB)
 * - 32 bits per pixel (RGBA). Alpha channel is ignored during processing.
 * 
 * Only uncompressed BMP files (BI_RGB) or BI_BITFIELDS (used only for 16-bit images) are supported.
 */
void drawBitmapFromSpiffs(const char *filename, int16_t x, int16_t y, bool with_color);

/**
 * Displays a raw binary image file from SPIFFS storage
 * Expects RGB565 format (16 bits per pixel)
 * File should contain raw pixel data without any header
 * Image dimensions must match the provided width and height parameters
 */
void drawProgmemFileFromSpiffs(const char *filename, uint16_t width, uint16_t height);

void imageRenderTask(void *parameter);

#endif
