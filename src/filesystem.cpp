#include "filesystem.h"
#include "image_utils.h"
#include "SPIFFS.h"
#include "config.h"
#include "display.h"

#include "debug.h"
#include <Arduino.h>

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    debug.println("[FILESYSTEM] Scanning directory: " + String(dirname));

    File root = fs.open(dirname);
    if (!root)
    {
        debug.println("[FILESYSTEM] Error: Directory access failed");
        return;
    }
    if (!root.isDirectory())
    {
        debug.println("[FILESYSTEM] Error: Path is not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            debug.println("  DIR : ");
            debug.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            debug.println("  FILE: ");
            debug.println(file.name());
            debug.println("\tSIZE: ");
            debug.println(String(file.size()));
        }
        file = root.openNextFile();
    }
}

String listFiles()
{
    debug.println("Listing files in SPIFFS...");
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
        debug.println("[FILESYSTEM] Starting new file upload: " + String(path.c_str()));
        SPIFFS.remove(path);
        f = SPIFFS.open(path, "w");
        if (!f)
        {
            debug.println("[FILESYSTEM] Error: Failed to create output file");
            request->send(500, "text/plain", "Internal Server Error");
            return;
        }
        totallength = 0;
        lastindex = 0;
    }

    if (len) // Something to write?
    {
        debug.println("[FILESYSTEM] Writing chunk of " + String(len) + " bytes to " + String(filename.c_str()));
        if ((index != lastindex) || (index == 0)) // New chunk?
        {
            f.write(data, len);
            totallength += len;
            lastindex = index;
            debug.println("Written " + String(len) + " bytes to " + String(filename.c_str()));
        }
    }

    if (final)
    {
        f.close();
        debug.println("[FILESYSTEM] Upload completed: " + String(filename.c_str()) + " (Total: " + String(totallength) + " bytes)\n");
        request->send(200, "OK", "File uploaded successfully");
        isImageRefreshPending = true;
        debug.println("Image refresh flag set.");
    }
    debug.println("[FILESYSTEM] Upload progress: " + String(totallength) + " bytes written");
}

void handleImageFileUpload(AsyncWebServerRequest *request, String filename,
                           size_t index, uint8_t *data, size_t len, bool final)
{
    String folder = String("/");
    filename = String(SELECTED_IMAGE_BUFFER_PATH);
    handleFileUpload(request, filename, index, data, len, final, folder);
}