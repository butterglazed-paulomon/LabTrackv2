#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#define CONFIG_FILE "/config.json"

bool loadConfig(Config &config) {
  if (!LittleFS.exists(CONFIG_FILE)) return false;
  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file) return false;

  StaticJsonDocument<512> doc;
  deserializeJson(doc, file);
  config.wifi_ssid = doc["wifi_ssid"] | "";
  config.wifi_password = doc["wifi_password"] | "";
  return true;
}

bool saveConfig(const Config &config) {
  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file) return false;
  StaticJsonDocument<512> doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  serializeJson(doc, file);
  return true;
}