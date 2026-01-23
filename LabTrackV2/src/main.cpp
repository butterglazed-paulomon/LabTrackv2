#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h> 
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// --- PINS (DO NOT CHANGE) ---
#define PN532_SCK  3
#define PN532_MISO 5
#define PN532_MOSI 7
#define PN532_SS   9

#define BUZZER_PIN 39
#define BLUE_LED_PIN 18  
#define GREEN_LED_PIN 37 
#define RED_LED_PIN 35   

// --- CONFIG ---
const char* AP_SSID = "LabTrackSystem";
const char* AP_PASS = "tinapay123";
const char* GAS_URL = "https://script.google.com/macros/s/AKfycbw3LLcA5mpJ40OX8VhsnDPoDDoHdv4DnFk_Y6-A2llTfQdowBeteFuDrQMAdXXt-09R/exec";

// --- OBJECTS ---
WebServer server(80);
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
WiFiClientSecure secureClient;

// --- FEEDBACK ---
void beep(int qty, int duration) {
  for (int i=0; i<qty; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(duration);
    digitalWrite(BUZZER_PIN, LOW); delay(50);
  }
}

// --- NFC PRIMITIVES ---
// Returns content string, or "" if empty/fail
String readCardData() {
  uint8_t uid[7], uidLen;
  // Fast timeout (100ms) for polling
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 100)) {
     uint8_t key[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
     if (nfc.mifareclassic_AuthenticateBlock(uid, uidLen, 4, 0, key)) {
        uint8_t data[16];
        if (nfc.mifareclassic_ReadDataBlock(4, data)) {
            String content = "";
            for(int i=0; i<16; i++) { if(data[i]==0) break; content += (char)data[i]; }
            return content;
        }
     }
  }
  return "";
}

bool writeCardData(String text) {
  uint8_t uid[7], uidLen;
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 500)) {
     uint8_t key[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
     if (nfc.mifareclassic_AuthenticateBlock(uid, uidLen, 4, 0, key)) {
        uint8_t data[16] = {0};
        for(int i=0; i<text.length() && i<16; i++) data[i] = text[i];
        return nfc.mifareclassic_WriteDataBlock(4, data);
     }
  }
  return false;
}

// --- GOOGLE SYNC ---
bool sendToGoogle(String uid, String type, String payload) {
  secureClient.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000); 
  
  if (!http.begin(secureClient, GAS_URL)) return false;
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;
  doc["uid"] = uid; doc["type"] = type;
  if(payload.length() > 0) {
      StaticJsonDocument<512> nested; deserializeJson(nested, payload); doc["payload"] = nested;
  }
  String json; serializeJson(doc, json);

  digitalWrite(BLUE_LED_PIN, LOW); // ON
  int code = http.POST(json);
  digitalWrite(BLUE_LED_PIN, HIGH); // OFF
  http.end();
  return (code == 200 || code == 302);
}

// --- SETUP ---
void setup() {
  delay(3000); Serial.begin(115200);

  pinMode(BLUE_LED_PIN, OUTPUT); digitalWrite(BLUE_LED_PIN, HIGH);
  pinMode(GREEN_LED_PIN, OUTPUT); digitalWrite(GREEN_LED_PIN, LOW);
  pinMode(RED_LED_PIN, OUTPUT); digitalWrite(RED_LED_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);

  LittleFS.begin();
  nfc.begin(); nfc.SAMConfig();

  // Load WiFi Config
  String ssid="", pass="";
  if(LittleFS.exists("/config.json")) {
    File f = LittleFS.open("/config.json", "r");
    StaticJsonDocument<256> doc; deserializeJson(doc, f);
    ssid = doc["ssid"] | ""; pass = doc["password"] | ""; f.close();
  }

  if(ssid == "") {
    WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID, AP_PASS);
  } else {
    WiFi.mode(WIFI_STA); WiFi.begin(ssid.c_str(), pass.c_str());
    unsigned long start = millis();
    while(WiFi.status() != WL_CONNECTED && millis()-start < 15000) delay(500);
    if(WiFi.status() == WL_CONNECTED) MDNS.begin("labtrack");
    else WiFi.softAP(AP_SSID, AP_PASS);
  }

  // --- ROUTES ---
  server.serveStatic("/", LittleFS, "/borrowform.html");
  server.serveStatic("/labstaff", LittleFS, "/labstaff.html");
  server.serveStatic("/config", LittleFS, "/config.html");
  server.serveStatic("/utility", LittleFS, "/utility.html");
  server.serveStatic("/borrowform.css", LittleFS, "/borrowform.css");

  // 1. GENERATE & WRITE (With Overwrite Protection)
  server.on("/generate", HTTP_POST, []() {
      if(!server.hasArg("plain")) return server.send(400, "text/plain", "Bad Request");
      String txID = "TX-" + String(random(1000, 9999));
      
      // Reply first so UI updates
      server.send(200, "application/json", "{\"success\":true, \"uid\":\""+txID+"\"}");
      
      Serial.println("Waiting for card...");
      unsigned long start = millis();
      bool success = false;
      int errorReason = 0; // 0=Timeout, 1=NotEmpty

      // Loop for 20 seconds
      while(millis() - start < 20000) {
          digitalWrite(RED_LED_PIN, HIGH); delay(100); digitalWrite(RED_LED_PIN, LOW);
          
          // A. Read First
          String existing = readCardData();
          
          // B. Check Logic
          if (existing.length() > 0 && existing != "0000000000000000") {
             // Card is NOT empty. Do not write.
             // Beep aggressively to warn user
             beep(2, 50); 
             errorReason = 1;
             delay(1000); // Wait for them to remove it
             continue; 
          }
          
          // C. If Empty (or we just treat blank string as empty), Write
          // Note: readCardData returns "" if read fails OR if blank (depending on implementation).
          // Ideally, we explicitly check for 0s. Assuming "" means unreadable or empty for now.
          if(writeCardData(txID)) { 
             success = true; break; 
          }
      }

      if(success) {
          beep(1, 200); digitalWrite(GREEN_LED_PIN, HIGH); delay(500); digitalWrite(GREEN_LED_PIN, LOW);
          sendToGoogle(txID, "borrow", server.arg("plain"));
      } else {
          beep(3, 100); // Error Beep
          if(errorReason == 1) Serial.println("Error: Card not empty");
      }
  });

  // 2. FINALIZE (Return & Wipe)
  server.on("/return/finalize", HTTP_POST, []() {
      StaticJsonDocument<512> doc; deserializeJson(doc, server.arg("plain"));
      String uid = doc["uid"].as<String>();
      server.send(200, "application/json", "{\"success\":true}");
      
      sendToGoogle(uid, "return_inspection", server.arg("plain"));
      
      // Wipe Loop (10s)
      unsigned long start = millis();
      while(millis() - start < 10000) {
          digitalWrite(RED_LED_PIN, HIGH); delay(100); digitalWrite(RED_LED_PIN, LOW);
          // Write Zeros
          if(writeCardData("")) { beep(3, 50); break; }
      }
  });

  // 3. UTILITY - TEST LEDS (Fixed JSON Error)
  server.on("/utility/test-led", HTTP_POST, []() {
      digitalWrite(RED_LED_PIN, HIGH); delay(200); digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH); delay(200); digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, LOW); delay(200); digitalWrite(BLUE_LED_PIN, HIGH);
      server.send(200, "application/json", "{\"message\":\"LEDs Tested\"}");
  });

  // 4. UTILITY - TEST BUZZER
  server.on("/utility/test-buzzer", HTTP_POST, []() {
      beep(2, 100);
      server.send(200, "application/json", "{\"message\":\"Buzzer Tested\"}");
  });

  // 5. UTILITY - READ (Used by LabStaff)
  server.on("/utility/read", HTTP_POST, []() {
      String c = readCardData();
      // Always return valid JSON, even if empty
      server.send(200, "application/json", "{\"content\":\""+c+"\"}");
  });

  server.on("/save-config", HTTP_POST, []() {
      File f = LittleFS.open("/config.json", "w");
      f.print(server.arg("plain")); f.close();
      server.send(200, "text/plain", "Saved.");
      delay(500); ESP.restart();
  });

  server.begin();
}

void loop() {
  server.handleClient();
  delay(2);
}