#pragma once
#include <Arduino.h>

void setupNFC();
void checkNFC();            // Main Polling Loop
void processGoogleQueue();  // Background Google Sync
void processManualCommands(); // Safe Manual Wipe

// Actions
bool wipeTag();
// Adds request to background queue
int sendWebhook(const String& uid, const String& actionType, String& responseBody);

// Utils
String manualReadNFC();
bool manualWriteNFC(const String& content);
bool manualWipeNFC();
void triggerManualWipe();   // Sets the flag