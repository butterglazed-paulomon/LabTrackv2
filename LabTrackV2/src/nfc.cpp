#include <Adafruit_PN532.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"
#include "storage.h"
#include "utils.h"
#include "nfc.h"
#include <LittleFS.h>
#include <queue>

// Define the Block to store the Transaction ID
#define BLOCK_TO_USE 4 

// Google Script URL (LabTrack Backend)
const char* GAS_URL = "https://script.google.com/macros/s/AKfycbw3LLcA5mpJ40OX8VhsnDPoDDoHdv4DnFk_Y6-A2llTfQdowBeteFuDrQMAdXXt-09R/exec";

extern Config config;
std::queue<String> pendingQueue;
unsigned long lastWriteTime = 0;
const unsigned long writeDebounceDelay = 2000; // 2 seconds
bool hasWrittenToCurrentCard = false;
String lastCardID = "";

// Global secure client to reuse SSL sessions (faster and saves RAM)
WiFiClientSecure secureClient;

// Hardware SPI Interface
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

uint8_t currentUid[7];
uint8_t currentUidLength = 0;
uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// --- Setup ---
void setupNFC() {
    nfc.begin();
    
    // Retry loop if PN532 is not found
    int retries = 0;
    while (!nfc.getFirmwareVersion()) {
        Serial.println("[WARN] PN532 not detected. Retrying...");
        delay(1000);
        nfc.begin();
        retries++;
        if (retries > 5) {
             Serial.println("[ERROR] PN532 Fatal Error. Check Wiring.");
             break;
        }
    }
    
    nfc.SAMConfig();
    Serial.println("[INFO] PN532 initialized.");
    setupFeedbackPins();
}

// --- Secure Webhook to Google Sheets ---
int sendWebhook(const String& uid, const String& actionType, String& responseBody) {
    // 1. Reset Connection
    secureClient.stop();
    secureClient.setInsecure(); // Skip certificate validation (Crucial for ESP32)

    HTTPClient http;

    // 2. Setup HTTP with Redirect Support (Crucial for Google Scripts)
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000); // 15s timeout for Google

    Serial.println("[Webhook] Connecting to Google...");

    if (!http.begin(secureClient, GAS_URL)) {
        Serial.println("[Webhook] Connection failed!");
        return -1;
    }

    // 3. Build JSON Payload
    ArduinoJson::StaticJsonDocument<512> doc;
    doc["uid"] = uid;
    doc["type"] = actionType;

    // If we have a dummy response passed in (like borrowing data), parse and include it
    if (responseBody.startsWith("{")) {
        DeserializationError error = deserializeJson(doc, responseBody);
        if (!error) responseBody = ""; 
    }

    String payload;
    serializeJson(doc, payload);

    http.addHeader("Content-Type", "application/json");

    // 4. Send POST
    int code = http.POST(payload);

    if (code > 0) {
        Serial.printf("[Webhook] Success: %d\n", code);
        // We do not download the body to save memory, unless strictly needed.
        // We assume success if Code is 200 or 302.
        responseBody = "{\"status\":\"sent\"}"; 
    } else {
        Serial.printf("[Webhook] Error: %s\n", http.errorToString(code).c_str());
    }

    http.end();
    secureClient.stop(); // Clean up SSL buffers
    return code;
}

// --- NFC Read/Write Helpers ---
bool authenticateBlock(uint8_t block) {
    return nfc.mifareclassic_AuthenticateBlock(currentUid, currentUidLength, block, 0, keya);
}

bool isTagBlank() {
    if (!authenticateBlock(BLOCK_TO_USE)) return false;

    uint8_t buffer[16];
    if (!nfc.mifareclassic_ReadDataBlock(BLOCK_TO_USE, buffer)) return false;

    for (int i = 0; i < 16; i++) {
        if (buffer[i] != 0x00) return false;
    }
    return true;
}

bool writeUIDToTag(const String& uid) {
    if (!authenticateBlock(BLOCK_TO_USE)) return false;

    uint8_t data[16] = { 0 };
    for (int i = 0; i < uid.length() && i < 16; i++) {
        data[i] = uid[i];
    }

    bool success = nfc.mifareclassic_WriteDataBlock(BLOCK_TO_USE, data);
    if (success) {
        Serial.println("[WRITE] Transaction ID written to tag.");
    } else {
        Serial.println("[ERROR] Write failed.");
    }
    return success;
}

String readUIDFromTag() {
    if (!authenticateBlock(BLOCK_TO_USE)) return "";

    uint8_t buffer[16];
    if (!nfc.mifareclassic_ReadDataBlock(BLOCK_TO_USE, buffer)) return "";

    String uid = "";
    for (int i = 0; i < 16; i++) {
        if (buffer[i] == 0x00) break;
        uid += (char)buffer[i];
    }
    return uid;
}

bool wipeTag() {
    // Authenticate and write zeros to Block 4
    if (!authenticateBlock(BLOCK_TO_USE)) return false;
    
    uint8_t blank[16] = {0};
    if (nfc.mifareclassic_WriteDataBlock(BLOCK_TO_USE, blank)) {
        Serial.println("[WIPE] Tag wiped successfully.");
        return true;
    }
    Serial.println("[ERROR] Failed to wipe tag.");
    return false;
}

// --- Main Logic Loop ---
void handleCardTap() {
    String hwUID = "";
    for (uint8_t i = 0; i < currentUidLength; i++) hwUID += String(currentUid[i], HEX);
    
    unsigned long now = millis();
    
    // Debounce: Ignore if same card tapped within 2 seconds
    if (hwUID == lastCardID && (now - lastWriteTime < writeDebounceDelay)) {
        return; 
    }
    lastCardID = hwUID;
    lastWriteTime = now;

    // 1. Check if Tag is Blank (Potential Borrower)
    if (isTagBlank()) {
        Serial.println("[NFC] Blank tag detected.");

        if (pendingQueue.empty()) {
            Serial.println("[INFO] No pending transactions. Tag ignored.");
            feedbackError(); // Red LED
            return;
        }

        // Get the oldest pending Transaction ID (e.g., TX-1001)
        String txID = pendingQueue.front();
        Serial.print("[DEBUG] Writing Transaction ID: ");
        Serial.println(txID);

        if (writeUIDToTag(txID)) {
            // Success! Notify Google that borrowing is confirmed (optional)
            String dummy = "";
            sendWebhook(txID, "confirm_borrow", dummy);
            
            feedbackSuccess(); // Green LED + Beep
            pendingQueue.pop(); // Remove from queue
            savePendingQueue(pendingQueue);
        } else {
            feedbackError();
        }
        return;
    }

    // 2. Tag has Data (Potential Return)
    String tagData = readUIDFromTag();
    Serial.print("[DEBUG] Scanned Receipt ID: ");
    Serial.println(tagData);

    if (tagData.length() > 0) {
        // Send to Google to check if this is a valid active transaction
        String response;
        int code = sendWebhook(tagData, "confirm_return", response);

        if (code == 200 || code == 302) {
            // We successfully told the backend "Here is card X"
            // The backend handles the "Look up" and displays it on the Dashboard.
            // We just give a local confirmation beep.
            Serial.println("[INFO] Sent to backend for inspection.");
            feedbackProcessing(); 
        } else {
            Serial.println("[ERROR] Failed to contact backend.");
            feedbackError();
        }
    } else {
        Serial.println("[WARN] Tag had non-zero data but read failed.");
        feedbackError();
    }
}

void checkNFC() {
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLength)) {
        digitalWrite(BLUE_LED_PIN, LOW); // LED ON
        handleCardTap();
        digitalWrite(BLUE_LED_PIN, HIGH); // LED OFF
        delay(1000); 
    }
}

// --- Utility Wrappers for Web Server ---
String manualReadNFC() { return readUIDFromTag(); }
bool manualWriteNFC(const String& content) { return writeUIDToTag(content); }
bool manualWipeNFC() { return wipeTag(); }