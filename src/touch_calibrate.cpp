// ============================================================
//  touch_calibrate.cpp
//  Run this sketch ONCE to calibrate the XPT2046 touch panel.
//  Follow the on-screen prompts — tap the crosshairs in each
//  corner when asked.
//
//  When done, copy the printed calibration array into
//  display.h at the TOUCH_CAL_DATA definition.
//
//  You do NOT need to run this sketch again unless you
//  change the display rotation or replace the hardware.
// ============================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(1);          // landscape — same as VFO firmware
    tft.fillScreen(TFT_BLACK);

    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Touch calibration", 160, 100, 2);
    tft.drawCentreString("Tap each crosshair", 160, 120, 2);
    delay(2000);

    uint16_t calData[5];
    tft.calibrateTouch(calData, TFT_WHITE, TFT_RED, 15);

    Serial.println("\n// Paste this into display.h:");
    Serial.print("#define TOUCH_CAL_DATA  { ");
    for (int i = 0; i < 5; i++) {
        Serial.print(calData[i]);
        if (i < 4) Serial.print(", ");
    }
    Serial.println(" }");

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString("Calibration complete!", 160, 100, 2);
    tft.drawCentreString("Check Serial Monitor", 160, 120, 2);
    tft.drawCentreString("for TOUCH_CAL_DATA", 160, 140, 2);
}

void loop() {}
