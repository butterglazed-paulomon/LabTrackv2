#include <Adafruit_PN532.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage.h"
#include "utils.h"
#include "nfc.h"
#include <LittleFS.h>
#include <queue>

#define BLOCK_TO_USE 4 
// ⚠️ ENSURE THIS URL IS YOUR GOOGLE SCRIPT URL
const char* GAS_URL = "https://script.google.com/macros/s/AKfycbw3LLcA5mpJ40OX8VhsnDPoDDoHdv4DnFk_Y6-A2llTfQdowBeteFuDrQMAdXXt-09R/exec";

std::queue<String> pendingQueue; // Write Queue

struct GoogleTask {
    String uid;
    String payload;
};
std::queue<GoogleTask> googleQueue; // Network Queue

volatile bool flagManualWipe = false; // Safe Wipe Flag
unsigned long lastWriteTime = 0;
String lastCardID = "";

WiFiClientSecure secureClient;
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
uint8_t currentUid[7];
uint8_t currentUidLength = 0;

void setupNFC() {
    nfc.begin();
    int retries = 0;
    while (!nfc.getFirmwareVersion()) {
        delay(500);
        nfc.begin();
        if(++retries > 3) break;
    }
    nfc.SAMConfig();
    nfc.setPassiveActivationRetries(0xFF);
    setupFeedbackPins();
}

// --- NETWORK ---
int sendWebhookInternal(const String& payload) {
    secureClient.setInsecure(); 
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(8000); 

    if (!http.begin(secureClient, GAS_URL)) return -1;
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    http.end();
    secureClient.stop(); 
    return code;
}

// Queue Processor
void processGoogleQueue() {
    if (googleQueue.empty()) return;
    GoogleTask task = googleQueue.front();
    
    Serial.println("[SYNC] Sending: " + task.uid);
    digitalWrite(BLUE_LED_PIN, LOW); // ON
    int code = sendWebhookInternal(task.payload);
    digitalWrite(BLUE_LED_PIN, HIGH); // OFF

    if (code == 200 || code == 302) {
        Serial.println("[SYNC] Done.");
        googleQueue.pop(); 
    } else {
        Serial.println("[SYNC] Fail. Dropping.");
        googleQueue.pop(); 
    }
}

// Public API
int sendWebhook(const String& uid, const String& actionType, String& responseBody) {
    // Reads are blocking
    if (actionType == "confirm_return" || actionType == "return_inspection") {
        ArduinoJson::StaticJsonDocument<512> doc;
        doc["uid"] = uid; doc["type"] = actionType;
        if (responseBody.startsWith("{")) deserializeJson(doc, responseBody);
        String payload; serializeJson(doc, payload);
        int code = sendWebhookInternal(payload);
        if (code > 0) responseBody = "{\"status\":\"sent\"}";
        return code;
    }

    // Writes are async
    GoogleTask task;
    task.uid = uid;
    ArduinoJson::StaticJsonDocument<512> doc;
    doc["uid"] = uid; doc["type"] = actionType;
    if (responseBody.startsWith("{")) deserializeJson(doc, responseBody);
    serializeJson(doc, task.payload);
    
    googleQueue.push(task);
    return 200; 
}

// --- NFC OPERATIONS ---
bool authenticateBlock(uint8_t block) {
    uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return nfc.mifareclassic_AuthenticateBlock(currentUid, currentUidLength, block, 0, key);
}

bool wipeTag() {
    if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLength)) return false;
    if (!authenticateBlock(BLOCK_TO_USE)) return false;
    uint8_t blank[16] = {0};
    if (nfc.mifareclassic_WriteDataBlock(BLOCK_TO_USE, blank)) {
        Serial.println("[WIPE] Success.");
        feedbackReturnAccepted();
        return true;
    }
    return false;
}

bool writeUIDToTag(const String& uid) {
    if (!authenticateBlock(BLOCK_TO_USE)) return false;
    uint8_t data[16] = { 0 };
    for (int i = 0; i < uid.length() && i < 16; i++) data[i] = uid[i];
    return nfc.mifareclassic_WriteDataBlock(BLOCK_TO_USE, data);
}

String readUIDFromTag() {
    if (!authenticateBlock(BLOCK_TO_USE)) return "";
    uint8_t buffer[16];
    if (!nfc.mifareclassic_ReadDataBlock(BLOCK_TO_USE, buffer)) return "";
    String uid = "";
    for (int i = 0; i < 16; i++) { if (buffer[i] == 0) break; uid += (char)buffer[i]; }
    return uid;
}

void triggerManualWipe() { flagManualWipe = true; }

void processManualCommands() {
    if (flagManualWipe) {
        Serial.println("[CMD] Manual Wipe Triggered...");
        for(int i=0; i<3; i++) { digitalWrite(RED_LED_PIN, HIGH); delay(100); digitalWrite(RED_LED_PIN, LOW); delay(100); }
        if (wipeTag()) feedbackSuccess();
        else feedbackError();
        flagManualWipe = false;
    }
}

// --- LOOP ---
void handleCardTap() {
    String hwUID = "";
    for (uint8_t i = 0; i < currentUidLength; i++) hwUID += String(currentUid[i], HEX);
    if (hwUID == lastCardID && (millis() - lastWriteTime < 2000)) return; 
    lastCardID = hwUID; lastWriteTime = millis();

    // 1. Check Blank (Write Mode)
    if (!authenticateBlock(BLOCK_TO_USE)) return;
    uint8_t buffer[16];
    if (nfc.mifareclassic_ReadDataBlock(BLOCK_TO_USE, buffer)) {
        bool isBlank = true;
        for(int i=0; i<16; i++) if(buffer[i] != 0) isBlank = false;

        if (isBlank) {
            if (!pendingQueue.empty()) {
                String txID = pendingQueue.front();
                Serial.println("[NFC] Writing: " + txID);
                if (writeUIDToTag(txID)) {
                    feedbackSuccess();
                    String dummy = "";
                    sendWebhook(txID, "confirm_borrow", dummy);
                    pendingQueue.pop(); savePendingQueue(pendingQueue);
                } else feedbackError();
            } else feedbackError();
            return;
        }

        // 2. Read (Return Mode)
        String content = "";
        for(int i=0; i<16; i++) { if(buffer[i]==0) break; content += (char)buffer[i]; }
        if (content.length() > 0) feedbackProcessing();
    }
}

void checkNFC() {
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLength)) {
        handleCardTap();
    }
}

// Wrappers
String manualReadNFC() { 
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLength)) return readUIDFromTag();
    return "";
}
bool manualWriteNFC(const String& content) { return false; } // Unused
bool manualWipeNFC() { return false; } // Unused