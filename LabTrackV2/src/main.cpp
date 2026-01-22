#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h> // <--- REQUIRED FOR mDNS
#include "config.h"
#include "storage.h"
#include "utils.h"
#include "app_server.h"
#include "nfc.h"

Config config;
AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  
  // 1. Filesystem & Config
  if(!LittleFS.begin()){
      Serial.println("LittleFS Failed");
      return;
  }
  loadConfig(config);
  
  // 2. WiFi Connection
  if (config.wifi_ssid == "") {
      // Fallback AP Mode
      WiFi.mode(WIFI_AP); 
      WiFi.softAP("LabTrackSystem", "tinapay123");
      Serial.println("AP Mode Started: LabTrackSystem");
  } else {
      // Station Mode
      WiFi.mode(WIFI_STA);
      WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
      
      unsigned long start = millis();
      while(WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
          delay(500); Serial.print(".");
      }
      
      if(WiFi.status() == WL_CONNECTED) {
          Serial.println("\nWiFi Connected!");
          Serial.println(WiFi.localIP());

          // 3. mDNS Setup
          if (MDNS.begin("catslabtrack")) {
              Serial.println("mDNS started -> http://catslabtrack.local");
          }
      } else {
          Serial.println("\nWiFi Failed. Starting AP.");
          WiFi.mode(WIFI_AP);
          WiFi.softAP("LabTrackSystem", "tinapay123");
      }
  }

  // 4. Start Services
  setupWebServer(server, config);
  server.begin();
  setupNFC();
  
  // 5. Load Queues
  extern std::queue<String> pendingQueue;
  loadPendingQueue(pendingQueue);
}

void loop() {
  checkNFC();
  processGoogleQueue();
  processManualCommands();
}