#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage.h"
#include "utils.h"
#include "app_server.h"
#include "nfc.h"
#include <LittleFS.h>
#include <queue>

extern std::queue<String> pendingQueue;

void setupWebServer(AsyncWebServer &server, Config &config) {
    
    // Serve Static Files
    server.serveStatic("/", LittleFS, "/").setDefaultFile("borrowform.html");

    // 1. GENERATE (Borrow)
    server.on("/generate", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<1024> doc; deserializeJson(doc, data);
            String txID = generateUID(); 

            // Reply to phone INSTANTLY
            StaticJsonDocument<256> res;
            res["success"] = true; res["uid"] = txID;
            String response; serializeJson(res, response);
            request->send(200, "application/json", response);

            // Queue Google Log
            doc["uid"] = txID; doc["type"] = "borrow";
            String payload; serializeJson(doc, payload);
            String dummy = payload;
            sendWebhook(txID, "borrow", dummy);

            // Queue Write
            pendingQueue.push(txID);
            savePendingQueue(pendingQueue);
        });

    // 2. FINALIZE (Return)
    server.on("/return/finalize", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<1024> doc; deserializeJson(doc, data);
            String uid = doc["uid"];

            // Queue Google Log
            String payload; serializeJson(doc, payload);
            String dummy = payload;
            sendWebhook(uid, "return_inspection", dummy);

            // Trigger Safe Wipe
            triggerManualWipe();

            request->send(200, "application/json", "{\"success\":true}");
        });

    // 3. UTILITY
    server.on("/utility/read", HTTP_POST, [](AsyncWebServerRequest *request) {
        String result = manualReadNFC();
        String json = "{\"content\":\"" + result + "\"}"; 
        request->send(200, "application/json", json);
    });

    server.on("/utility/wipe", HTTP_POST, [](AsyncWebServerRequest *request) {
        triggerManualWipe();
        request->send(200, "application/json", "{\"status\":\"Wipe Scheduled\"}");
    });

    // Config
    server.on("/save-config", HTTP_POST, [&config](AsyncWebServerRequest *request) {}, nullptr,
        [&config](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<512> doc; deserializeJson(doc, data);
            config.wifi_ssid = doc["ssid"] | "";
            config.wifi_password = doc["password"] | "";
            saveConfig(config);
            request->send(200, "text/plain", "Saved. Rebooting...");
            delay(1000); ESP.restart();
        });
}