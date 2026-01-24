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
    debug.println("[FILESYSTEM] handleFileUpload called - index: " + String(index) + ", len: " + String(len) + ", final: " + String(final));

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
        debug.println("[FILESYSTEM] Upload completed: " + String(filename.c_str()) + " (Total: " + String(totallength) + " bytes)");

        // Wait a moment for SPIFFS to flush
        delay(100);

        // Verify the file is readable and has correct size
        debug.println("[FILESYSTEM] Verifying uploaded file...");
        path = folder + filename;
        File verifyFile = SPIFFS.open(path, "r");
        if (!verifyFile) {
            debug.println("[FILESYSTEM] ERROR: Cannot open file for verification!");
            request->send(500, "text/plain", "File verification failed");
            return;
        }

        size_t fileSize = verifyFile.size();
        debug.println("[FILESYSTEM] Verified file size: " + String(fileSize) + " bytes");

        // Try reading first few bytes to ensure file is readable
        uint8_t testBuffer[128];
        size_t testRead = verifyFile.read(testBuffer, sizeof(testBuffer));
        verifyFile.close();

        debug.println("[FILESYSTEM] Test read: " + String(testRead) + " bytes from start of file");

        if (testRead == 0) {
            debug.println("[FILESYSTEM] ERROR: File appears empty or unreadable!");
            request->send(500, "text/plain", "File unreadable");
            return;
        }

        debug.println("[FILESYSTEM] File verification successful");
        debug.println("[FILESYSTEM] Setting image refresh flag...");
        isImageRefreshPending = true;
        debug.println("[FILESYSTEM] Image refresh flag set to TRUE");
        request->send(200, "text/plain", "Upload complete");
        debug.println("[FILESYSTEM] Response sent to client");
    }
    debug.println("[FILESYSTEM] Upload progress: " + String(totallength) + " bytes written");
}

void handleImageFileUpload(AsyncWebServerRequest *request, String filename,
                           size_t index, uint8_t *data, size_t len, bool final)
{
    // SPIFFS is mounted at /data
    String folder = String("/data/");
    filename = String(SELECTED_IMAGE_BUFFER_PATH);
    handleFileUpload(request, filename, index, data, len, final, folder);
}