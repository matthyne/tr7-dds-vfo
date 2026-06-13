// ============================================================
//  display.h — TR7 DDS VFO display driver
//  Wraps TFT_eSPI and XPT2046 touch into a clean interface.
//  All drawing is double-buffered via sprite where possible
//  so the frequency display doesn't flicker on update.
// ============================================================
#pragma once
#include <TFT_eSPI.h>
#include <SPI.h>
#include "config.h"
#include "memory.h"

// ------------------------------------------------------------
//  Colour palette — dark radio theme
//  TFT_eSPI uses 16-bit RGB565 colours.
//  Use the tft.color565(r,g,b) helper to convert.
// ------------------------------------------------------------
// Backgrounds
#define C_BG_SCREEN     0x0000    // #000000 — main background
#define C_BG_TOPBAR     0x0841    // #080808 — top bar
#define C_BG_CELL       0x0821    // #080410 — band/mem cell bg
#define C_BG_CELLACT    0x0228    // #002850 — active cell bg (blue tint)
#define C_BG_CELLCB     0x1008    // #100010 — active CB cell bg (purple)
#define C_BG_CELLMAR    0x0010    // #000020 — active marine cell bg

// Foregrounds
#define C_FREQ_MAIN     0xEF7D    // #E8E6E0 — large frequency digits
#define C_FREQ_UNIT     0x8410    // #808080 — "MHz" label
#define C_VFO_SUB       0x4208    // #404040 — VFO sub-line
#define C_FTW_SUB       0x2104    // #202020 — FTW sub-line
#define C_LABEL         0x2945    // #282828 — section labels

// Accent colours
#define C_TEAL          0x07F4    // #00F880 — active/ham green
#define C_BLUE          0x1C9F    // #1C4CF8 — marine blue
#define C_PURPLE        0x681F    // #6810F8 — CB purple
#define C_AMBER         0xF400    // #F48000 — RIT orange
#define C_RED           0xE800    // #E00000 — lock red
#define C_GRAY          0x4208    // #404040 — inactive

// Step/band button states
#define C_BTN_BG        0x0821
#define C_BTN_BORDER    0x18C3
#define C_BTN_ACT_BG    0x0228
#define C_BTN_ACT_TXT   0x07FF    // cyan-ish
#define C_BTN_CB_BG     0x1008
#define C_BTN_CB_TXT    0xB81F
#define C_BTN_MAR_BG    0x0010
#define C_BTN_MAR_TXT   0x1C9F

// S-meter segments
#define C_SMETER_S      0x07E0    // green
#define C_SMETER_9      0xFD20    // amber
#define C_SMETER_P      0xF800    // red
#define C_SMETER_OFF    0x1082

// Screen layout constants (landscape 320x240)
#define SCR_W           320
#define SCR_H           240

#define TOPBAR_H         24
#define FREQZONE_Y       24
#define FREQZONE_H       60
#define RITBAR_Y         84     // shown/hidden depending on RIT state
#define RITBAR_H         18
#define STEPROW_Y       102
#define STEPROW_H        22
#define BANDZONE_Y      124
#define BANDZONE_H       26
#define MEMZONE_Y       150
#define MEMZONE_H        52
#define BOTBAR_Y        210     // SCR_H - BOTBAR_H
#define BOTBAR_H         30

#define MARGIN           8      // horizontal margin

// ------------------------------------------------------------
//  Touch calibration constants
//  Run the calibration sketch once, read the values from
//  serial output, and paste them here.
//  Default values below are approximate for the CYD —
//  replace with your measured values for accuracy.
// ------------------------------------------------------------
#define TOUCH_CAL_DATA  { 339, 3498, 237, 3560, 7 }
// Format: { x_min, x_max, y_min, y_max, rotation_flag }

// ------------------------------------------------------------
//  Touch IRQ pin
// ------------------------------------------------------------
#define TOUCH_IRQ       36      // GPIO36 (input only, no pull-up needed)

// ------------------------------------------------------------
//  Display class
// ------------------------------------------------------------
class VFODisplay {
public:
    TFT_eSPI    tft;
    TFT_eSprite sprFreq   = TFT_eSprite(&tft);   // frequency zone sprite
    TFT_eSprite sprStatus = TFT_eSprite(&tft);   // top bar sprite

    bool        ritVisible = false;

    void begin() {
        tft.init();
        tft.setRotation(1);                      // landscape, USB top-left
        tft.fillScreen(C_BG_SCREEN);

        // Backlight on
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

        // Initialise sprites
        sprFreq.createSprite(SCR_W - 2*MARGIN, FREQZONE_H);
        sprFreq.setColorDepth(16);

        sprStatus.createSprite(SCR_W, TOPBAR_H);
        sprStatus.setColorDepth(16);

        // Touch calibration
        uint16_t cal[5] = TOUCH_CAL_DATA;
        tft.setTouch(cal);

        drawStaticChrome();
    }

    // Draw elements that don't change (divider lines, labels)
    void drawStaticChrome() {
        // Top bar background
        tft.fillRect(0, 0, SCR_W, TOPBAR_H, C_BG_TOPBAR);
        // Divider lines
        tft.drawFastHLine(0, TOPBAR_H,    SCR_W, C_BTN_BORDER);
        tft.drawFastHLine(0, STEPROW_Y,   SCR_W, C_BTN_BORDER);
        tft.drawFastHLine(0, BANDZONE_Y,  SCR_W, C_BTN_BORDER);
        tft.drawFastHLine(0, MEMZONE_Y,   SCR_W, C_BTN_BORDER);
        tft.drawFastHLine(0, BOTBAR_Y,    SCR_W, C_BTN_BORDER);
        // "dial frequency" label
        tft.setTextFont(1);
        tft.setTextSize(1);
        tft.setTextColor(C_LABEL, C_BG_SCREEN);
        tft.drawString("dial frequency", MARGIN, FREQZONE_Y + 3);
    }

    // Update the large frequency display using a sprite (no flicker)
    void updateFrequency(float dialMHz, const char* chTag, uint16_t chTagCol,
                         float vfoMHz, uint32_t ftwVal, bool ritActive,
                         float ritKHz) {
        sprFreq.fillSprite(C_BG_SCREEN);

        // Large frequency digits — Font 7 gives 7-seg style with decimal
        sprFreq.setTextFont(7);
        sprFreq.setTextSize(1);
        sprFreq.setTextColor(C_FREQ_MAIN, C_BG_SCREEN);

        char buf[16];
        // Format: XX.XXX or X.XXXX depending on magnitude
        if (dialMHz >= 10.0f)
            snprintf(buf, sizeof(buf), "%.3f", dialMHz);
        else
            snprintf(buf, sizeof(buf), "%.4f", dialMHz);

        sprFreq.drawString(buf, 0, 8);

        // "MHz" unit label
        int tw = sprFreq.textWidth(buf);
        sprFreq.setTextFont(2);
        sprFreq.setTextColor(C_FREQ_UNIT, C_BG_SCREEN);
        sprFreq.drawString("MHz", tw + 4, 24);

        // Channel tag badge (CB channel or marine channel)
        if (chTag && strlen(chTag) > 0) {
            int bx = tw + 4;
            int by = 8;
            sprFreq.setTextFont(2);
            int bw = sprFreq.textWidth(chTag) + 8;
            sprFreq.fillRoundRect(bx, by, bw, 16, 3, chTagCol);
            sprFreq.setTextColor(TFT_WHITE, chTagCol);
            sprFreq.drawString(chTag, bx + 4, by + 2);
        }

        // VFO sub-line
        sprFreq.setTextFont(1);
        sprFreq.setTextColor(C_VFO_SUB, C_BG_SCREEN);
        snprintf(buf, sizeof(buf), "VFO %.6f MHz", vfoMHz);
        sprFreq.drawString(buf, 0, 46);

        // RIT offset
        if (ritActive && fabsf(ritKHz) > 0.001f) {
            char ritBuf[20];
            snprintf(ritBuf, sizeof(ritBuf), "  RIT %+.2f kHz", ritKHz);
            sprFreq.setTextColor(C_AMBER, C_BG_SCREEN);
            sprFreq.drawString(ritBuf, sprFreq.textWidth(buf), 46);
        }

        // FTW sub-line
        sprFreq.setTextColor(C_FTW_SUB, C_BG_SCREEN);
        snprintf(buf, sizeof(buf), "FTW 0x%08lX", (unsigned long)ftwVal);
        sprFreq.drawString(buf, 0, 54);

        sprFreq.pushSprite(MARGIN, FREQZONE_Y + 12);
    }

    // Update the top bar — mode, badges, status
    void updateTopBar(const char* mode, bool ritOn, bool locked,
                      uint16_t modeBgCol, const char* memLabel) {
        sprStatus.fillSprite(C_BG_TOPBAR);

        // Mode pill
        int px = MARGIN;
        sprStatus.setTextFont(2);
        int mw = sprStatus.textWidth(mode) + 10;
        sprStatus.fillRoundRect(px, 4, mw, 16, 3, modeBgCol);
        sprStatus.setTextColor(TFT_WHITE, modeBgCol);
        sprStatus.drawString(mode, px + 5, 6);
        px += mw + 6;

        // RIT badge
        if (ritOn) {
            sprStatus.fillRoundRect(px, 4, 26, 16, 3, C_BG_SCREEN);
            sprStatus.drawRoundRect(px, 4, 26, 16, 3, C_AMBER);
            sprStatus.setTextColor(C_AMBER, C_BG_SCREEN);
            sprStatus.drawString("RIT", px + 3, 6);
            px += 32;
        }

        // Lock badge
        if (locked) {
            sprStatus.fillRoundRect(px, 4, 34, 16, 3, C_BG_SCREEN);
            sprStatus.drawRoundRect(px, 4, 34, 16, 3, C_RED);
            sprStatus.setTextColor(C_RED, C_BG_SCREEN);
            sprStatus.drawString("LOCK", px + 3, 6);
            px += 40;
        }

        // Memory label
        if (memLabel && strlen(memLabel) > 0) {
            int mlw = sprStatus.textWidth(memLabel) + 10;
            sprStatus.fillRoundRect(px, 4, mlw, 16, 3, 0x1008);
            sprStatus.drawRoundRect(px, 4, mlw, 16, 3, C_PURPLE);
            sprStatus.setTextColor(C_PURPLE, 0x1008);
            sprStatus.drawString(memLabel, px + 5, 6);
        }

        // Right side: EXT VFO status dot + label
        sprStatus.fillCircle(SCR_W - 38, 12, 3, C_TEAL);
        sprStatus.setTextColor(C_GRAY, C_BG_TOPBAR);
        sprStatus.drawString("EXT VFO", SCR_W - 32, 6);

        sprStatus.pushSprite(0, 0);
    }

    // Draw step rate row — 6 buttons across full width
    void drawStepRow(int activeIdx) {
        const char* labels[] = {"1Hz","10Hz","100Hz","1kHz","10kHz","100kHz"};
        int bw = (SCR_W - 2*MARGIN - 5*2) / 6;
        for (int i = 0; i < 6; i++) {
            int x = MARGIN + i*(bw+2);
            int y = STEPROW_Y + 3;
            bool act = (i == activeIdx);
            tft.fillRoundRect(x, y, bw, STEPROW_H-6, 3,
                              act ? C_BTN_ACT_BG : C_BTN_BG);
            tft.drawRoundRect(x, y, bw, STEPROW_H-6, 3,
                              act ? C_TEAL : C_BTN_BORDER);
            tft.setTextFont(1);
            tft.setTextColor(act ? C_TEAL : C_GRAY, act ? C_BTN_ACT_BG : C_BTN_BG);
            int tw = tft.textWidth(labels[i]);
            tft.drawString(labels[i], x + (bw-tw)/2, y + 4);
        }
    }

    // Draw band buttons — 11 bands in 2 rows of 6/5
    // bandIdx 0-10, cbIdx=8, marIdx=10
    void drawBandZone(int activeIdx, int cbIdx, int marIdx) {
        const char* labels[] = {
            "160m","80m","40m","30m","20m","17m","15m","12m","11m","10m","Mar"
        };
        int cols = 6;
        int bw = (SCR_W - 2*MARGIN - (cols-1)*2) / cols;
        for (int i = 0; i < 11; i++) {
            int row = i / cols;
            int col = i % cols;
            int x = MARGIN + col*(bw+2);
            int y = BANDZONE_Y + 3 + row * 13;
            bool act = (i == activeIdx);
            bool isCB  = (i == cbIdx);
            bool isMar = (i == marIdx);

            uint16_t bg  = act ? (isCB  ? C_BTN_CB_BG :
                                  isMar ? C_BTN_MAR_BG : C_BTN_ACT_BG)
                               : C_BTN_BG;
            uint16_t bdr = act ? (isCB  ? C_PURPLE :
                                  isMar ? C_BLUE   : C_TEAL)
                               : C_BTN_BORDER;
            uint16_t txt = act ? (isCB  ? C_BTN_CB_TXT  :
                                  isMar ? C_BTN_MAR_TXT  : C_BTN_ACT_TXT)
                               : C_GRAY;

            tft.fillRoundRect(x, y, bw, 11, 2, bg);
            tft.drawRoundRect(x, y, bw, 11, 2, bdr);
            tft.setTextFont(1);
            tft.setTextColor(txt, bg);
            int tw = tft.textWidth(labels[i]);
            tft.drawString(labels[i], x + (bw-tw)/2, y + 2);
        }
    }

    // Draw memory zone — bank tabs A-J, then 10 cells
    void drawMemZone(int activeBank, int activeCell,
                     MemSlot* bank, int bankCount) {
        // Bank tabs
        int tabW = (SCR_W - 2*MARGIN - 9*2) / 10;
        for (int i = 0; i < 10; i++) {
            char label[2] = {(char)('A'+i), 0};
            int x = MARGIN + i*(tabW+2);
            int y = MEMZONE_Y + 2;
            bool act = (i == activeBank);
            tft.fillRect(x, y, tabW, 10,
                         act ? 0x1008 : C_BTN_BG);
            tft.drawRect(x, y, tabW, 10,
                         act ? C_PURPLE : C_BTN_BORDER);
            tft.setTextFont(1);
            tft.setTextColor(act ? C_PURPLE : C_GRAY,
                             act ? 0x1008 : C_BTN_BG);
            tft.drawCentreString(label, x + tabW/2, y + 1, 1);
        }

        // Memory cells
        int cellW = (SCR_W - 2*MARGIN - 9*2) / 10;
        for (int i = 0; i < 10; i++) {
            int x = MARGIN + i*(cellW+2);
            int y = MEMZONE_Y + 14;
            bool act   = (i == activeCell);
            bool filled = (bank && bank[i].valid);

            uint16_t bg  = act ? 0x1008 : (filled ? 0x0821 : C_BTN_BG);
            uint16_t bdr = act ? C_PURPLE : (filled ? 0x2228 : C_BTN_BORDER);

            tft.fillRoundRect(x, y, cellW, 34, 2, bg);
            tft.drawRoundRect(x, y, cellW, 34, 2, bdr);

            tft.setTextFont(1);
            if (filled) {
                // Frequency (small) — dial freq = VFO + band offset
                tft.setTextColor(act ? C_PURPLE : C_GRAY, bg);
                char freq[10];
                float dialMHz = (bank[i].vfoHz + BANDS[bank[i].bandIdx].offset) / 1e6f;
                snprintf(freq, sizeof(freq), "%.2f", dialMHz);
                int fw = tft.textWidth(freq);
                tft.drawString(freq, x + (cellW-fw)/2, y + 3);
                // Label
                tft.setTextColor(act ? C_BTN_CB_TXT : 0x4208, bg);
                // Truncate label to fit
                char lbl[7];
                strncpy(lbl, bank[i].label, 6); lbl[6]=0;
                int lw = tft.textWidth(lbl);
                tft.drawString(lbl, x + (cellW-lw)/2, y + 14);
                // Mode
                tft.setTextColor(0x2945, bg);
                int mw = tft.textWidth(bank[i].mode);
                tft.drawString(bank[i].mode, x + (cellW-mw)/2, y + 24);
            } else {
                // Empty slot — show index
                char idx[3];
                snprintf(idx, sizeof(idx), "%d", i);
                tft.setTextColor(0x1863, bg);
                tft.drawCentreString(idx, x + cellW/2, y + 13, 1);
            }
        }
    }

    // Draw S-meter (12 segments, x from right side of bottom bar)
    void drawSMeter(float level) {
        // level 0.0 – 12.0
        int x0 = SCR_W - MARGIN - 12*6 - 14;
        int y  = BOTBAR_Y + 10;
        tft.setTextFont(1);
        tft.setTextColor(C_GRAY, C_BG_TOPBAR);
        tft.drawString("S", x0-8, y+1);
        for (int i = 0; i < 12; i++) {
            bool lit = (i < (int)level);
            uint16_t col = lit ? (i < 7 ? C_SMETER_S :
                                  i < 10 ? C_SMETER_9 : C_SMETER_P)
                               : C_SMETER_OFF;
            tft.fillRect(x0 + i*6, y, 5, 8, col);
        }
    }

    // Draw the function buttons row in the bottom bar
    void drawBotBar(bool ritOn, bool locked, bool scanOn) {
        tft.fillRect(0, BOTBAR_Y+1, SCR_W, BOTBAR_H-1, C_BG_TOPBAR);
        const char* labels[] = {"RIT","LOCK","SCAN","M-SAVE"};
        bool        states[] = {ritOn, locked, scanOn, false};
        uint16_t    cols[]   = {C_AMBER, C_RED, C_TEAL, C_GRAY};
        int bw = 52, bh = 18, gap = 4;
        for (int i = 0; i < 4; i++) {
            int x = MARGIN + i*(bw+gap);
            int y = BOTBAR_Y + 6;
            bool act = states[i];
            tft.fillRoundRect(x, y, bw, bh, 3,
                              act ? (cols[i] & 0x1082) : C_BTN_BG);
            tft.drawRoundRect(x, y, bw, bh, 3,
                              act ? cols[i] : C_BTN_BORDER);
            tft.setTextFont(1);
            tft.setTextColor(act ? cols[i] : C_GRAY,
                             act ? (cols[i] & 0x1082) : C_BTN_BG);
            int tw = tft.textWidth(labels[i]);
            tft.drawString(labels[i], x + (bw-tw)/2, y + 4);
        }
    }

    // Touch: returns true if a touch is detected, fills x/y
    bool getTouch(uint16_t* x, uint16_t* y) {
        return tft.getTouch(x, y);
    }
};
