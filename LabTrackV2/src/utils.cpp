#include "utils.h"

String generateUID() {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  String uid = "CATS-";
  for (int i = 0; i < 6; i++) uid += charset[random(0, 35)];
  return uid;
}

void setupFeedbackPins() {
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
}

void beep(int duration) {
    digitalWrite(BUZZER_PIN, HIGH); delay(duration); digitalWrite(BUZZER_PIN, LOW);
}

void feedbackSuccess() {
    digitalWrite(GREEN_LED_PIN, HIGH); beep(100); delay(100); beep(100); delay(1000); digitalWrite(GREEN_LED_PIN, LOW);
}

void feedbackError() {
    digitalWrite(RED_LED_PIN, HIGH); beep(1000); delay(1000); digitalWrite(RED_LED_PIN, LOW);
}

void feedbackProcessing() {
    digitalWrite(BLUE_LED_PIN, LOW); delay(100); digitalWrite(BLUE_LED_PIN, HIGH);
}

void feedbackReturnAccepted() {
    for(int i=0; i<3; i++) {
        digitalWrite(GREEN_LED_PIN, HIGH); beep(50); delay(50); digitalWrite(GREEN_LED_PIN, LOW); delay(50);
    }
}

void runLEDTest() {
    digitalWrite(RED_LED_PIN, HIGH); delay(200); digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH); delay(200); digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW); delay(200); digitalWrite(BLUE_LED_PIN, HIGH);
}

void runBuzzerTest() { beep(100); }