// ============================================================
//  display_test.cpp
//  Smoke test — run this before building the full VFO firmware.
//  Verifies:
//    1. TFT_eSPI config is correct (display initialises)
//    2. Correct rotation (320 wide x 240 tall)
//    3. Touch is reading (prints coords to serial)
//    4. Correct colours (visual check)
//    5. Font rendering at VFO-relevant sizes
//    6. Sprite (double-buffer) operation
//    7. SPI shared bus doesn't corrupt display when touch reads
//
//  Expected result: dark screen, large green frequency digits,
//  band buttons, S-meter bar. Touching the screen prints
//  calibrated x/y to serial and highlights the touched region.
// ============================================================
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "display.h"   // for TOUCH_CAL_DATA

TFT_eSPI    tft;
TFT_eSprite spr = TFT_eSprite(&tft);

// Minimal colour set matching display.h
#define C_BG      0x0000
#define C_GREEN   0x07E4
#define C_GRAY    0x4208
#define C_AMBER   0xF400
#define C_BORDER  0x18C3

void setup() {
    Serial.begin(115200);
    Serial.println("\nTR7 DDS VFO — display test");

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(C_BG);

    // Backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Check dimensions
    Serial.printf("Display W=%d H=%d\n", tft.width(), tft.height());
    if (tft.width() != 320 || tft.height() != 240) {
        tft.setTextColor(TFT_RED, C_BG);
        tft.drawString("ERROR: wrong dimensions!", 10, 10, 2);
        Serial.println("ERROR: expected 320x240 in landscape");
        while(1) delay(1000);
    }

    // ── Test 1: Large frequency digits using Font 7 ──────────
    tft.setTextColor(C_GREEN, C_BG);
    tft.drawString("14.175", 8, 28, 7);
    tft.setTextColor(C_GRAY, C_BG);
    tft.drawString("MHz", 8, 90, 2);

    // ── Test 2: Subline fonts ─────────────────────────────────
    tft.setTextColor(C_GRAY, C_BG);
    tft.drawString("VFO 5.175000 MHz", 8, 106, 1);
    tft.drawString("FTW 0x07A1B43C", 8, 116, 1);

    // ── Test 3: Band buttons ──────────────────────────────────
    const char* bands[] = {"160m","80m","40m","30m","20m","17m","15m","12m","11m","10m","Mar"};
    int bw = 26, bh = 12;
    for (int i = 0; i < 11; i++) {
        int row = i/6, col = i%6;
        int x = 8 + col*(bw+2), y = 130 + row*14;
        bool act = (i == 4);
        tft.fillRoundRect(x, y, bw, bh, 2, act ? 0x0228 : 0x0821);
        tft.drawRoundRect(x, y, bw, bh, 2, act ? C_GREEN : C_BORDER);
        tft.setTextColor(act ? C_GREEN : C_GRAY, act ? 0x0228 : 0x0821);
        tft.drawCentreString(bands[i], x+bw/2, y+2, 1);
    }

    // ── Test 4: S-meter ───────────────────────────────────────
    tft.setTextColor(C_GRAY, C_BG);
    tft.drawString("S", 212, 160, 1);
    for (int i = 0; i < 12; i++) {
        uint16_t col = i < 7 ? 0x07E0 : i < 10 ? 0xFD20 : 0xF800;
        bool lit = (i < 6);
        tft.fillRect(222 + i*6, 158, 5, 10, lit ? col : 0x1082);
    }

    // ── Test 5: Sprite double-buffer ─────────────────────────
    spr.createSprite(200, 20);
    spr.fillSprite(C_BG);
    spr.setTextColor(C_AMBER, C_BG);
    spr.drawString("Sprite OK - no flicker", 0, 4, 2);
    spr.pushSprite(8, 175);

    // ── Test 6: Touch calibration check ──────────────────────
    uint16_t cal[5] = TOUCH_CAL_DATA;
    tft.setTouch(cal);

    tft.setTextColor(C_GRAY, C_BG);
    tft.drawString("Touch screen to test", 8, 200, 1);
    tft.drawString("Cal: see display.h TOUCH_CAL_DATA", 8, 210, 1);

    Serial.println("Static draw complete — touching screen will print coords");
}

void loop() {
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty)) {
        Serial.printf("Touch: x=%d y=%d\n", tx, ty);
        // Flash a small dot at touch point
        tft.fillCircle(tx, ty, 3, C_AMBER);
        delay(80);
        tft.fillCircle(tx, ty, 3, C_BG);
    }
    delay(10);
}
