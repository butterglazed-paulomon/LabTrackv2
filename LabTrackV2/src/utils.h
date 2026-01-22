#pragma once
#include <Arduino.h>

// PIN MAPPING (Lolin S2 Mini / ESP32)
#define PN532_SCK  3
#define PN532_MISO 5
#define PN532_MOSI 7
#define PN532_SS   9

#define BUZZER_PIN 39
#define BLUE_LED_PIN 18  // Status / Network
#define GREEN_LED_PIN 37 // Success
#define RED_LED_PIN 35   // Error / Wipe Mode

String generateUID(); 
void setupFeedbackPins();
void feedbackSuccess();
void feedbackError();
void feedbackProcessing();
void feedbackReturnAccepted();
void runLEDTest();
void runBuzzerTest();