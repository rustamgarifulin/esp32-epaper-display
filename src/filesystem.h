#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <FS.h>
#include <ESPAsyncWebServer.h>

extern volatile bool uploadSuccess;
extern const char* uploadErrorMessage;

void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
String listFiles();
void handleFileUpload(AsyncWebServerRequest *request, String filename,
                      size_t index, uint8_t *data, size_t len, bool final, String folder);
void handleImageFileUpload(AsyncWebServerRequest *request, String filename,
                           size_t index, uint8_t *data, size_t len, bool final);

#endif