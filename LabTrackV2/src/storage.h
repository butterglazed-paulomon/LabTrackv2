#pragma once
#include <Arduino.h>
#include <queue>

// Saves the queue of Transaction IDs (for writing) to memory
bool savePendingQueue(const std::queue<String>& queue);
bool loadPendingQueue(std::queue<String>& queue);