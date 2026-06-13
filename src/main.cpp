// ============================================================
//  main.cpp — TR7 DDS VFO main firmware
//  ESP32-2432S028 (Cheap Yellow Display)
//
//  Architecture: two-core ESP32
//    Core 0 (loop()):  display updates, touch, scan timer
//    Core 1 (setup()): DDS writes and encoder ISR
//    A FreeRTOS mutex guards vfoState access between cores.
//
//  Module summary:
//    config.h    — all pin assignments and constants
//    bands.h     — CB/marine channel tables
//    dds.h       — AD9851 bit-bang driver
//    memory.h    — NVS memory channel storage
//    vfo_state.h — central state + all transitions
//    display.h   — TFT_eSPI display driver wrapper
//    touch_ui.h  — touch zone hit-testing
// ============================================================
#include <Arduino.h>
#include <ESP32Encoder.h>
#include "config.h"
#include "bands.h"
#include "dds.h"
#include "memory.h"
#include "vfo_state.h"
#include "display.h"
#include "touch_ui.h"

// ── Module instances ─────────────────────────────────────────
AD9851         dds;
MemoryManager  mem;
VFOState       state;
VFODisplay     disp;
TouchUI        touchUI;
ESP32Encoder   encoder;

// ── FreeRTOS mutex — protects state across cores ──────────────
SemaphoreHandle_t stateMutex;
#define TAKE_STATE()  xSemaphoreTake(stateMutex, portMAX_DELAY)
#define GIVE_STATE()  xSemaphoreGive(stateMutex)

// ── Scan timer ────────────────────────────────────────────────
uint32_t lastScanMs = 0;

// ── S-meter simulation (replace with real AGC read if wired) ──
float   smLevel    = 4.0f;
float   smTarget   = 4.0f;
uint32_t lastSmMs  = 0;

// ── Mode tracking ─────────────────────────────────────────────
uint32_t lastModeMs = 0;

// ── Display timers ────────────────────────────────────────────
uint32_t lastFreqMs = 0;
uint32_t lastFullMs = 0;

// ============================================================
//  Encoder handling — encoder ISR runs on Core 1 via Arduino
//  ESP32Encoder library handles debounce and quadrature.
// ============================================================
void IRAM_ATTR encoderISR() {
    // Handled internally by ESP32Encoder — nothing needed here
}

// ============================================================
//  Mode tracking — read TR7 mode switch lines
// ============================================================
uint8_t readTR7Mode() {
    // Each line is grounded by the TR7 mode switch.
    // Return index: 0=USB 1=LSB 2=CW 3=AM 4=FM
    // If MODE_FM == -1, FM is not wired — skip it.
    struct { int pin; uint8_t idx; } lines[] = {
        { MODE_USB, 0 },
        { MODE_LSB, 1 },
        { MODE_CW,  2 },
        { MODE_AM,  3 },
        { MODE_FM,  4 },
    };
    for (auto& l : lines) {
        if (l.pin < 0) continue;
        if (digitalRead(l.pin) == LOW) return l.idx;
    }
    return state.modeIdx;   // no change if nothing grounded
}

// ============================================================
//  S-meter — animate or read real AGC voltage
//  If you wire the TR7's S-meter AGC line to an ADC pin,
//  replace this with: return analogRead(SMETER_PIN) / 341.0f;
//  (maps 0–4095 ADC range to 0–12 S-meter segments)
// ============================================================
float readSMeter() {
    // Simulated random walk — replace with real AGC read
    smTarget += (random(0, 100) - 50) / 50.0f;
    smTarget  = constrain(smTarget, 0.5f, 11.0f);
    smLevel  += (smTarget - smLevel) * 0.25f;
    return smLevel;
}

// ============================================================
//  Display refresh — only redraws what has changed
// ============================================================
void refreshDisplay() {
    uint32_t now = millis();

    // Full redraw (band zone, step row, bottom bar, memory zone)
    if (state.dirtyFull || (now - lastFullMs > DISPLAY_FULL_MS)) {
        disp.drawStepRow(state.stepIdx);
        disp.drawBandZone(state.bandIdx, CB_BAND_IDX, MAR_BAND_IDX);
        disp.drawMemZone(state.memBank, state.activeMem,
                         mem.banks[state.memBank], MEM_SLOTS_PER_BANK);
        disp.drawBotBar(state.ritActive, state.locked, state.scanning);
        state.dirtyFull   = false;
        state.dirtyBotBar = false;
        lastFullMs = now;
    }

    // Bottom bar only (RIT/lock/scan toggled)
    if (state.dirtyBotBar) {
        disp.drawBotBar(state.ritActive, state.locked, state.scanning);
        state.dirtyBotBar = false;
    }

    // Frequency zone — fast update via sprite
    if (state.dirtyFreq || (now - lastFreqMs > DISPLAY_FREQ_MS)) {
        // Build channel tag string
        char chTag[8] = {0};
        uint16_t chTagCol = 0x1008;
        uint32_t edHz = state.effectiveVFO() + BANDS[state.bandIdx].offset;

        if (BANDS[state.bandIdx].isCB) {
            int idx = cbFindChannel(edHz);
            if (idx >= 0) {
                cbChannelLabel(idx, chTag);
                chTagCol = 0x1008;   // purple background
            }
        } else if (BANDS[state.bandIdx].isMarine) {
            int idx = marFindChannel(edHz);
            if (idx >= 0) {
                marChannelLabel(idx, chTag);
                chTagCol = 0x0010;   // blue background
            }
        }

        disp.updateFrequency(
            state.effectiveDialMHz(),
            chTag, chTagCol,
            state.effectiveVFO() / 1e6f,
            AD9851::calcFTW(state.effectiveVFO()),
            state.ritActive,
            state.ritKHz()
        );

        // Top bar (mode, badges)
        char memLbl[4];
        state.memLabel(memLbl);
        disp.updateTopBar(
            MODE_NAMES[state.modeIdx],
            state.ritActive,
            state.locked,
            state.ritActive ? 0xF400 :   // amber when RIT
            0x07E0,                       // green otherwise
            memLbl
        );

        state.dirtyFreq = false;
        lastFreqMs = now;
    }

    // S-meter
    if (now - lastSmMs > DISPLAY_SMETER_MS) {
        disp.drawSMeter(readSMeter());
        lastSmMs = now;
    }
}

// ============================================================
//  Touch event dispatch
// ============================================================
void handleTouch() {
    uint16_t tx, ty;
    bool touched = disp.getTouch(&tx, &ty);
    TouchEvent evt = touchUI.process(touched, tx, ty);

    if (evt.action == TouchAction::None) return;

    TAKE_STATE();

    switch (evt.action) {

        case TouchAction::StepSelect:
            state.setStepIdx(evt.data);
            break;

        case TouchAction::BandSelect:
            state.setBand(evt.data, dds);
            break;

        case TouchAction::MemTabSelect:
            state.memBank  = evt.data;
            state.dirtyFull = true;
            break;

        case TouchAction::MemCellTap: {
            uint8_t slot = evt.data;
            if (mem.isValid(state.memBank, slot)) {
                state.recallMemory(state.memBank, slot, mem, dds);
            } else {
                // Tap empty cell — save current frequency there
                state.saveToMemory(state.memBank, slot, mem);
            }
            break;
        }

        case TouchAction::MemCellLong:
            // Long press — always saves, even over existing memory
            state.saveToMemory(state.memBank, evt.data, mem);
            break;

        case TouchAction::BtnRIT:
            state.toggleRIT(dds);
            break;

        case TouchAction::BtnLock:
            state.toggleLock();
            break;

        case TouchAction::BtnScan:
            state.scanning = !state.scanning;
            state.dirtyBotBar = true;
            if (!state.scanning) lastScanMs = 0;
            break;

        case TouchAction::BtnMemSave: {
            // Save to first empty slot in active bank, or slot 0 if full
            int slot = 0;
            for (int s = 0; s < MEM_SLOTS_PER_BANK; s++) {
                if (!mem.isValid(state.memBank, s)) { slot = s; break; }
            }
            state.saveToMemory(state.memBank, slot, mem);
            break;
        }

        case TouchAction::ModeToggle:
            // Touch-cycle mode (overrides TR7 tracking temporarily)
            state.setMode((state.modeIdx + 1) % NUM_MODES);
            break;

        default: break;
    }

    GIVE_STATE();
}

// ============================================================
//  Encoder processing — called from main loop
// ============================================================
void handleEncoder() {
    int32_t ticks = encoder.getCount();
    if (ticks == 0) return;
    encoder.clearCount();

    // Acceleration: if turning fast, multiply step
    static uint32_t lastEncMs = 0;
    static int32_t  tickAccum = 0;
    uint32_t now = millis();
    uint32_t dt  = now - lastEncMs;
    lastEncMs    = now;

    // Count ticks per 100ms window
    tickAccum += abs(ticks);
    if (dt > 100) tickAccum = abs(ticks);

    int32_t multiplier = 1;
    if (tickAccum > ENC_ACCEL_THRESHOLD) multiplier = ENC_ACCEL_FACTOR;

    // Normalise quadrature — ESP32Encoder counts per pulse, not per detent
    int32_t detents = ticks / ENC_STEPS_PER_DETENT;
    if (detents == 0 && ticks != 0) detents = (ticks > 0) ? 1 : -1;

    TAKE_STATE();
    state.tune(detents * multiplier, dds);
    GIVE_STATE();
}

// ============================================================
//  Encoder button — short press cycles step, long press resets RIT
// ============================================================
void handleEncoderButton() {
    static bool     lastState    = HIGH;
    static uint32_t pressStart   = 0;
    static bool     longFired    = false;

    bool btn = digitalRead(ENC_BTN);

    if (btn == LOW && lastState == HIGH) {
        pressStart = millis();
        longFired  = false;
    }

    if (btn == LOW && !longFired && (millis() - pressStart > 800)) {
        // Long press — clear RIT
        TAKE_STATE();
        state.clearRIT(dds);
        GIVE_STATE();
        longFired = true;
    }

    if (btn == HIGH && lastState == LOW && !longFired) {
        // Short press — cycle step rate up
        TAKE_STATE();
        state.setStepIdx((state.stepIdx + 1) % NUM_STEPS);
        GIVE_STATE();
    }

    lastState = btn;
}

// ============================================================
//  Scan — steps through filled memories in active bank
// ============================================================
void handleScan() {
    if (!state.scanning) return;
    uint32_t now = millis();
    if (now - lastScanMs < SCAN_DWELL_MS) return;
    lastScanMs = now;

    TAKE_STATE();
    int next = mem.nextFilled(state.memBank, state.activeMem);
    if (next >= 0) {
        state.recallMemory(state.memBank, next, mem, dds);
    } else {
        state.scanning = false;   // No filled slots — stop scan
        state.dirtyBotBar = true;
    }
    GIVE_STATE();
}

// ============================================================
//  Mode tracking — poll TR7 mode switch lines
// ============================================================
void handleModeTracking() {
    uint32_t now = millis();
    if (now - lastModeMs < MODE_POLL_MS) return;
    lastModeMs = now;

    uint8_t mode = readTR7Mode();
    TAKE_STATE();
    state.setMode(mode);
    GIVE_STATE();
}

// ============================================================
//  setup()
// ============================================================
void setup() {
    Serial.begin(115200);
    Serial.println("TR7 DDS VFO starting...");

    // Create mutex before anything that might use state
    stateMutex = xSemaphoreCreateMutex();

    // Mode tracking pins
    int modePins[] = { MODE_USB, MODE_LSB, MODE_CW, MODE_AM, MODE_FM };
    for (int p : modePins) {
        if (p >= 0) pinMode(p, INPUT_PULLUP);
    }

    // Encoder button
    pinMode(ENC_BTN, INPUT_PULLUP);

    // DDS
    dds.begin();
    Serial.printf("DDS sysclk: %lu Hz, resolution: %.4f mHz/LSB\n",
                  DDS_SYSCLK_HZ, AD9851::resolution() * 1000.0f);

    // Memory
    mem.begin();
    Serial.printf("Memory loaded. Bank A has %d filled slots.\n",
                  mem.countFilled(0));

    // Encoder — uses PCNT hardware counters, handles debounce
    ESP32Encoder::useInternalWeakPullResistors = puType::UP;
    encoder.attachHalfQuad(ENC_A, ENC_B);
    encoder.clearCount();

    // Display
    disp.begin();
    touchUI.begin();

    // Set initial DDS frequency
    dds.setFrequency(state.vfoHz);

    Serial.println("Setup complete.");
    Serial.printf("Initial VFO: %.6f MHz, dial: %.3f MHz\n",
                  state.vfoHz / 1e6f, state.dialMHz());
}

// ============================================================
//  loop() — runs on Core 0
// ============================================================
void loop() {
    handleEncoder();
    handleEncoderButton();
    handleTouch();
    handleScan();
    handleModeTracking();
    refreshDisplay();

    // Yield to FreeRTOS scheduler — keeps watchdog happy
    vTaskDelay(1);
}
