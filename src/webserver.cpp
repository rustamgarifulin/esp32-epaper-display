#include "webserver.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <Arduino.h>
#include <display.h>
#include <SPIFFS.h>
#include "debug.h"

#include "filesystem.h"

AsyncWebServer webServer(80);

String getFullMemoryUsage() {
    debug.println("[MEMORY] Generating system memory report");
    String usage = "Total heap: ";
    usage += String(ESP.getHeapSize());
    usage += "\nFree heap: ";
    usage += String(ESP.getFreeHeap());
    usage += "\nTotal PSRAM: ";
    usage += String(ESP.getPsramSize());
    usage += "\nFree PSRAM: ";
    usage += String(ESP.getFreePsram());
    debug.println("[MEMORY] Memory report generated successfully");
    return usage;
}

void notFoundResponse(AsyncWebServerRequest *request) {
    debug.println("[WEBSERVER] Error 404: Resource not found at " + String(request->url().c_str()));
    request->send(404, "text/plain", "Not found");
}

void startWebserver() {
    debug.println("[WEBSERVER] Initializing web server");

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        debug.println("[WEBSERVER] Processing root path request");
        request->send(200, "text/plain", "Hello, world");
    });

    webServer.on("/api/system/memory", HTTP_GET, [](AsyncWebServerRequest *request) {
        debug.println("[WEBSERVER] Received GET request on '/api/system/memory'");
        request->send(200, "text/plain", getFullMemoryUsage());
    });

    webServer.on("/api/system/list", HTTP_GET, [](AsyncWebServerRequest *request) {
        debug.println("[WEBSERVER] Received GET request on '/api/system/list'");
        request->send(200, "text/plain", listFiles());
    });

    webServer.on("/api/image/draw", HTTP_GET, [](AsyncWebServerRequest *request) {
        debug.println("[WEBSERVER] Received GET request on '/api/image/draw'");
        request->send(200, "text/plain", "Drawing saved image");
        isImageRefreshPending = true;
    });

    webServer.on(
        "/api/image/upload",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            debug.println("[WEBSERVER] Received POST request on '/api/image/upload'");
            request->send(200, "text/plain", "Upload complete");
        },
        handleImageFileUpload
    );

    webServer.serveStatic("/fs", SPIFFS, "/");
    debug.println("[WEBSERVER] Static file serving enabled");

    webServer.onNotFound(notFoundResponse);
    debug.println("[WEBSERVER] 404 handler registered");

    debug.println("[WEBSERVER] Configuration complete, starting server");
    webServer.begin();
    debug.println("[WEBSERVER] Server is now listening on port 80");
}
