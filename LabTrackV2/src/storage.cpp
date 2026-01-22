#include "storage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#define PENDING_QUEUE_FILE "/pending_queue.json"

bool savePendingQueue(const std::queue<String>& queue) {
    File file = LittleFS.open(PENDING_QUEUE_FILE, "w");
    if (!file) return false;
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();
    std::queue<String> temp = queue;
    while (!temp.empty()) {
        arr.add(temp.front());
        temp.pop();
    }
    serializeJson(doc, file);
    return true;
}

bool loadPendingQueue(std::queue<String>& queue) {
    if (!LittleFS.exists(PENDING_QUEUE_FILE)) return false;
    File file = LittleFS.open(PENDING_QUEUE_FILE, "r");
    if (!file) return false;
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, file);
    JsonArray arr = doc.as<JsonArray>();
    for (JsonVariant v : arr) queue.push(v.as<String>());
    return true;
}