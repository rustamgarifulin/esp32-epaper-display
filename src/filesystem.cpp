#include "filesystem.h"
#include "image_utils.h"
#include "SPIFFS.h"
#include "config.h"
#include "display.h"

#include <Arduino.h>

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("[FILESYSTEM] Scanning directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("[FILESYSTEM] Error: Directory access failed");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println("[FILESYSTEM] Error: Path is not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

String listFiles()
{
    Serial.println("Listing files in SPIFFS...");
    listDir(SPIFFS, "/", 0);
    return ("Directory listing sent to Serial.");
}

void handleFileUpload(AsyncWebServerRequest *request, String filename,
                      size_t index, uint8_t *data, size_t len, bool final, String folder)
{
    String path;
    static File f;
    static uint32_t totallength;
    static size_t lastindex;

    if (index == 0)
    {
        path = folder + filename;
        Serial.printf("[FILESYSTEM] Starting new file upload: %s\n", path.c_str());
        SPIFFS.remove(path);        // Delete old file
        f = SPIFFS.open(path, "w"); // Create new file
        if (!f)
        {
            Serial.println("[FILESYSTEM] Error: Failed to create output file");
            request->send(500, "text/plain", "Internal Server Error");
            return;
        }
        totallength = 0;
        lastindex = 0;
    }

    if (len) // Something to write?
    {
        Serial.printf("[FILESYSTEM] Writing chunk of %u bytes to %s\n", len, filename.c_str());
        if ((index != lastindex) || (index == 0)) // New chunk?
        {
            f.write(data, len);
            totallength += len;
            lastindex = index;
            Serial.printf("Written %u bytes to %s\n", len, filename.c_str());
        }
    }

    if (final)
    {
        f.close();
        Serial.printf("[FILESYSTEM] Upload completed: %s (Total: %u bytes)\n", filename.c_str(), totallength);
        request->send(200, "OK", "File uploaded successfully");
        isImageRefreshPending = true;
        Serial.println("Image refresh flag set.");
    }
    Serial.printf("[FILESYSTEM] Upload progress: %u bytes written\n", totallength);
}

void handleImageFileUpload(AsyncWebServerRequest *request, String filename,
                           size_t index, uint8_t *data, size_t len, bool final)
{
    String folder = String("/");
    filename = String(SELECTED_IMAGE_BUFFER_PATH);
    handleFileUpload(request, filename, index, data, len, final, folder);
}