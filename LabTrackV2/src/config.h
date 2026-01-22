#pragma once
#include <Arduino.h>

struct Config {
  String wifi_ssid;
  String wifi_password;
  String flask_ip; // Kept for legacy compatibility
};

bool loadConfig(Config &config);
bool saveConfig(const Config &config);