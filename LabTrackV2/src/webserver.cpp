#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "config.h"
#include "storage.h"
#include "utils.h"
#include "webserver.h"
#include "nfc.h"
#include <LittleFS.h>
#include <queue>

// Include HTML headers (Make sure these files exist in your include path or src folder)
#include "labstaff_css.h"
#include "borrowform_css.h"

extern Config config;
extern std::queue<String> pendingQueue; // Defined in nfc.cpp
extern String lastCardID; // Used to identify which card is currently on the reader

// We need to reference the function from nfc.cpp
extern int sendWebhook(const String& uid, const String& actionType, String& responseBody);

void setupWebServer(AsyncWebServer &server, Config &config) {

    // ==========================================
    //  HTML PAGE ROUTES
    // ==========================================
    
    // 1. Student Borrow Form
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/borrowform.html", "text/html");
    });

    // 2. Staff Return/Inspection Dashboard
    server.on("/labstaff", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/labstaff.html", "text/html");
    });

    // 3. Configuration & Tools
    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/config.html", "text/html");
    });

    server.on("/utility", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/utility.html", "text/html");
    });

    // CSS Routes
    server.on("/labstaff.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/css", labstaff_css);
    });

    server.on("/borrowform.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/css", borrowform_css);
    });


    // ==========================================
    //  API ROUTES (The Logic)
    // ==========================================

    // --- A. GENERATE TRANSACTION (Borrow Start) ---
    // User clicks "Generate" on Phone -> ESP32 creates ID -> Sends to Google -> Waits for Tag
    server.on("/generate", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, data);
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            // 1. Generate a unique Receipt ID (TX-XXXXXX)
            String txID = generateUID(); 

            // 2. Add the ID to the payload and prepare for Google
            doc["uid"] = txID;
            // The type 'borrow' tells your Google Script to save this to the Transactions tab
            doc["type"] = "borrow"; 

            String payload;
            serializeJson(doc, payload);

            // 3. Send to Google immediately (Fire and Forget)
            // We reuse the payload string as a carrier for the response body logic
            String dummyResponse = payload; 
            int code = sendWebhook(txID, "borrow", dummyResponse);

            if (code != 200 && code != 302) {
                 request->send(500, "application/json", "{\"success\":false, \"message\":\"Google Sync Failed\"}");
                 return;
            }

            // 4. Queue the ID to be written to the NFC Card
            pendingQueue.push(txID);
            savePendingQueue(pendingQueue); // Save to LittleFS in case of reboot

            // 5. Reply to the Web Interface
            StaticJsonDocument<256> res;
            res["success"] = true;
            res["uid"] = txID;
            res["message"] = "Transaction Logged. TAP CARD NOW to write ID.";
            
            String response;
            serializeJson(res, response);
            request->send(200, "application/json", response);
        });

    // --- B. FINALIZE RETURN (Staff Inspection Complete) ---
    // Staff checks items -> Clicks "Confirm" -> ESP32 sends report -> Wipes Tag
    server.on("/return/finalize", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, data);
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            String uid = doc["uid"];
            if (uid == "") {
                request->send(400, "application/json", "{\"error\":\"Missing UID\"}");
                return;
            }

            // 1. Forward the inspection report to Google
            // Payload should look like: { "type": "return_inspection", "uid": "...", "item_statuses": [...] }
            String payload;
            serializeJson(doc, payload); // The frontend sends the exact structure Google needs
            
            String googleResponse = payload;
            int code = sendWebhook(uid, "return_inspection", googleResponse);

            if (code == 200 || code == 302) {
                // 2. If Google confirms receipt, WIPE the tag
                if (wipeTag()) {
                    feedbackReturnAccepted(); // 3 beeps
                    request->send(200, "application/json", "{\"success\":true, \"message\":\"Return Processed & Tag Wiped\"}");
                } else {
                    feedbackError();
                    request->send(500, "application/json", "{\"success\":false, \"message\":\"Google OK, but Tag Wipe Failed\"}");
                }
            } else {
                request->send(500, "application/json", "{\"success\":false, \"message\":\"Failed to send report to Google\"}");
            }
        });

    // --- C. UTILITY ROUTES ---

    // Manual Read (for debugging)
    server.on("/utility/read", HTTP_POST, [](AsyncWebServerRequest *request) {
        String result = manualReadNFC();
        StaticJsonDocument<128> doc;
        doc["content"] = result;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Manual Wipe
    server.on("/utility/wipe", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool success = manualWipeNFC();
        request->send(200, "application/json", success ?
            "{\"status\":\"Wipe successful\"}" : "{\"status\":\"Wipe failed\"}");
    });

    // Test Hardware
    server.on("/utility/test-led", HTTP_POST, [](AsyncWebServerRequest *request) {
        runLEDTest();
        request->send(200, "application/json", "{\"message\":\"LED test triggered\"}");
    });

    server.on("/utility/test-buzzer", HTTP_POST, [](AsyncWebServerRequest *request) {
        runBuzzerTest();
        request->send(200, "application/json", "{\"message\":\"Buzzer test triggered\"}");
    });

    // Config Saver
    server.on("/save-config", HTTP_POST, [&config](AsyncWebServerRequest *request) {}, nullptr,
        [&config](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, data);
            if (error) { request->send(400, "text/plain", "Invalid JSON"); return; }

            config.wifi_ssid = doc["ssid"] | "";
            config.wifi_password = doc["password"] | "";
            // We don't really use flask_ip anymore, but keeping it prevents config errors
            config.flask_ip = doc["flask_ip"] | "";

            if (saveConfig(config)) {
                request->send(200, "text/plain", "Config saved. Rebooting...");
                delay(2000);
                ESP.restart();
            } else {
                request->send(500, "text/plain", "Failed to save config.");
            }
        });

    // Serve Static Files
    server.serveStatic("/", LittleFS, "/").setDefaultFile("borrowform.html");
    
    server.on("/config.json", HTTP_GET, [&config](AsyncWebServerRequest *request) {
        StaticJsonDocument<128> doc;
        doc["flask_ip"] = config.flask_ip; 
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}