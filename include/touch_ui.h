// ============================================================
//  touch_ui.h — Touch zone layout and hit-testing
//  Maps physical touch coordinates to UI actions.
//  All zone geometry mirrors display.h layout constants.
// ============================================================
#pragma once
#include <Arduino.h>
#include "config.h"
#include "vfo_state.h"
#include "memory.h"

// Touch debounce — ignore touches within this many ms of last
#define TOUCH_DEBOUNCE_MS   120
// Long-press threshold for memory save
#define TOUCH_LONG_PRESS_MS MEM_LONG_PRESS_MS

// Screen layout (landscape 320x240) — mirrors display.h
#define T_SCR_W     320
#define T_SCR_H     240
#define T_MARGIN      8
#define T_TOPBAR_H   24
#define T_STEPROW_Y 102
#define T_STEPROW_H  22
#define T_BANDZONE_Y 124
#define T_BANDZONE_H  26
#define T_MEMTAB_Y  150
#define T_MEMTAB_H   12
#define T_MEMCELL_Y 162
#define T_MEMCELL_H  38
#define T_BOTBAR_Y  210
#define T_BOTBAR_H   30

enum class TouchAction {
    None,
    StepSelect,     // data = step index 0-5
    BandSelect,     // data = band index 0-10
    MemTabSelect,   // data = bank index 0-9
    MemCellTap,     // data = slot index 0-9
    MemCellLong,    // data = slot index 0-9 (long press = save)
    BtnRIT,
    BtnLock,
    BtnScan,
    BtnMemSave,
    ModeToggle,
};

struct TouchEvent {
    TouchAction action = TouchAction::None;
    int         data   = 0;
};

class TouchUI {
public:
    void begin() {
        _lastTouchMs   = 0;
        _pressStartMs  = 0;
        _pressSlot     = -1;
        _pressing      = false;
    }

    // Call every loop iteration. Returns a TouchEvent if something actionable happened.
    // Pass raw touch x,y from tft.getTouch() and whether touch is currently active.
    TouchEvent process(bool touched, uint16_t tx, uint16_t ty) {
        uint32_t now = millis();
        TouchEvent evt;

        if (!touched) {
            if (_pressing) {
                // Released — check if it was a long press on a mem cell
                if (_pressSlot >= 0 && (now - _pressStartMs) < TOUCH_LONG_PRESS_MS) {
                    // Short tap on mem cell
                    evt.action = TouchAction::MemCellTap;
                    evt.data   = _pressSlot;
                }
                _pressing  = false;
                _pressSlot = -1;
            }
            return evt;
        }

        // Debounce
        if (now - _lastTouchMs < TOUCH_DEBOUNCE_MS && !_pressing) return evt;

        // Long press check on mem cells
        if (_pressing && _pressSlot >= 0) {
            if (now - _pressStartMs >= TOUCH_LONG_PRESS_MS) {
                evt.action = TouchAction::MemCellLong;
                evt.data   = _pressSlot;
                _pressing  = false;
                _pressSlot = -1;
                _lastTouchMs = now;
            }
            return evt;
        }

        if (_pressing) return evt;  // Wait for release on other zones

        _lastTouchMs = now;

        // ── Top bar ───────────────────────────────────────────
        if (ty < T_TOPBAR_H) {
            // Mode pill is left ~50px
            if (tx < 60) {
                evt.action = TouchAction::ModeToggle;
                return evt;
            }
            return evt;
        }

        // ── Step row ──────────────────────────────────────────
        if (ty >= T_STEPROW_Y && ty < T_STEPROW_Y + T_STEPROW_H) {
            int bw = (T_SCR_W - 2*T_MARGIN - 5*2) / 6;
            int col = (tx - T_MARGIN) / (bw + 2);
            if (col >= 0 && col < 6) {
                evt.action = TouchAction::StepSelect;
                evt.data   = col;
            }
            return evt;
        }

        // ── Band zone ─────────────────────────────────────────
        if (ty >= T_BANDZONE_Y && ty < T_BANDZONE_Y + T_BANDZONE_H) {
            int cols = 6;
            int bw   = (T_SCR_W - 2*T_MARGIN - (cols-1)*2) / cols;
            int col  = (tx - T_MARGIN) / (bw + 2);
            int row  = (ty - T_BANDZONE_Y) < 13 ? 0 : 1;
            int idx  = row * cols + col;
            if (idx >= 0 && idx < NUM_BANDS) {
                evt.action = TouchAction::BandSelect;
                evt.data   = idx;
            }
            return evt;
        }

        // ── Memory bank tabs ──────────────────────────────────
        if (ty >= T_MEMTAB_Y && ty < T_MEMTAB_Y + T_MEMTAB_H) {
            int tw  = (T_SCR_W - 2*T_MARGIN - 9*2) / 10;
            int col = (tx - T_MARGIN) / (tw + 2);
            if (col >= 0 && col < 10) {
                evt.action = TouchAction::MemTabSelect;
                evt.data   = col;
            }
            return evt;
        }

        // ── Memory cells ─────────────────────────────────────
        if (ty >= T_MEMCELL_Y && ty < T_MEMCELL_Y + T_MEMCELL_H) {
            int cw  = (T_SCR_W - 2*T_MARGIN - 9*2) / 10;
            int col = (tx - T_MARGIN) / (cw + 2);
            if (col >= 0 && col < 10) {
                // Start press timer — resolved on release or long-press timeout
                _pressing      = true;
                _pressStartMs  = now;
                _pressSlot     = col;
            }
            return evt;
        }

        // ── Bottom bar buttons ────────────────────────────────
        if (ty >= T_BOTBAR_Y) {
            int bw = 52, gap = 4;
            int col = (tx - T_MARGIN) / (bw + gap);
            switch (col) {
                case 0: evt.action = TouchAction::BtnRIT;     break;
                case 1: evt.action = TouchAction::BtnLock;    break;
                case 2: evt.action = TouchAction::BtnScan;    break;
                case 3: evt.action = TouchAction::BtnMemSave; break;
                default: break;
            }
            return evt;
        }

        return evt;
    }

private:
    uint32_t _lastTouchMs  = 0;
    uint32_t _pressStartMs = 0;
    int      _pressSlot    = -1;
    bool     _pressing     = false;
};
