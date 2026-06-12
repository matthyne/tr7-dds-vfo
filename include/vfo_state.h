// ============================================================
//  vfo_state.h — Central VFO state
//  All mutable state lives here. No global variables elsewhere.
//  State transitions are methods — never modify fields directly
//  from outside this struct.
// ============================================================
#pragma once
#include <Arduino.h>
#include "config.h"
#include "bands.h"
#include "dds.h"
#include "memory.h"

const char* const MODE_NAMES[] = { "USB", "LSB", "CW", "AM", "FM" };
const int NUM_MODES = 5;

struct VFOState {
    // ── Frequency ─────────────────────────────────────────────
    uint32_t vfoHz       = 5100000;     // Current VFO freq (no RIT)
    uint8_t  bandIdx     = 4;           // Current band (default 20m)
    uint32_t stepHz      = 1000;        // Current tuning step
    uint8_t  stepIdx     = 3;           // Index into STEP_RATES[]

    // ── RIT ───────────────────────────────────────────────────
    bool     ritActive   = false;
    int32_t  ritHz       = 0;           // RIT offset (±RIT_MAX_HZ)

    // ── Mode ─────────────────────────────────────────────────
    uint8_t  modeIdx     = 0;           // 0=USB 1=LSB 2=CW 3=AM 4=FM

    // ── UI state ─────────────────────────────────────────────
    bool     locked      = false;
    bool     scanning    = false;
    uint8_t  memBank     = 0;           // Active memory bank (0=A … 9=J)
    int8_t   activeMem   = -1;          // Active memory slot (-1 = none)
    bool     memDirty    = false;       // true = freq moved since recall

    // ── Display dirty flags — set true to trigger redraw ──────
    bool     dirtyFreq   = true;
    bool     dirtyFull   = true;        // full screen redraw needed
    bool     dirtyBotBar = true;

    // ── Derived ───────────────────────────────────────────────
    uint32_t dialHz() const {
        return (uint32_t)((int32_t)vfoHz + BANDS[bandIdx].offset);
    }

    float dialMHz() const {
        return dialHz() / 1e6f;
    }

    uint32_t effectiveVFO() const {
        int32_t v = (int32_t)vfoHz + (ritActive ? ritHz : 0);
        if (v < (int32_t)VFO_MIN_HZ) v = VFO_MIN_HZ;
        if (v > (int32_t)VFO_MAX_HZ) v = VFO_MAX_HZ;
        return (uint32_t)v;
    }

    float effectiveDialMHz() const {
        return (effectiveVFO() + BANDS[bandIdx].offset) / 1e6f;
    }

    float ritKHz() const { return ritHz / 1000.0f; }

    // ── Step management ───────────────────────────────────────
    void setStepIdx(uint8_t idx) {
        if (idx >= NUM_STEPS) return;
        stepIdx = idx;
        stepHz  = STEP_RATES[idx];
        dirtyFull = true;
    }

    // ── Tuning ───────────────────────────────────────────────
    // Returns true if frequency actually changed
    bool tune(int32_t ticks, AD9851& dds) {
        if (locked) return false;

        if (ritActive) {
            // Encoder tunes RIT while RIT is active
            ritHz += ticks * RIT_STEP_HZ;
            ritHz  = constrain(ritHz, -RIT_MAX_HZ, RIT_MAX_HZ);
            dds.setFrequencyWithRIT(vfoHz, ritHz);
            dirtyFreq = true;
            return true;
        }

        int32_t delta  = ticks * (int32_t)stepHz;
        int32_t newVFO = (int32_t)vfoHz + delta;
        newVFO = constrain(newVFO,
                           (int32_t)BANDS[bandIdx].vfoMin,
                           (int32_t)BANDS[bandIdx].vfoMax);

        if ((uint32_t)newVFO == vfoHz) return false;

        vfoHz = (uint32_t)newVFO;
        dds.setFrequency(vfoHz);

        activeMem = -1;     // Moving off a memory position
        memDirty  = true;
        dirtyFreq = true;
        return true;
    }

    // ── Band change ───────────────────────────────────────────
    void setBand(uint8_t idx, AD9851& dds) {
        if (idx >= NUM_BANDS) return;
        bandIdx   = idx;
        vfoHz     = BANDS[idx].defaultVFO;
        activeMem = -1;

        // CB band: force 10 kHz step
        if (BANDS[idx].isCB) setStepIdx(4);

        dds.setFrequency(vfoHz);
        dirtyFull  = true;
        dirtyFreq  = true;
        dirtyBotBar = true;
    }

    // ── RIT ───────────────────────────────────────────────────
    void toggleRIT(AD9851& dds) {
        ritActive = !ritActive;
        if (!ritActive) {
            ritHz = 0;
            dds.setFrequency(vfoHz);
        }
        dirtyFreq   = true;
        dirtyBotBar = true;
        dirtyFull   = true;
    }

    void clearRIT(AD9851& dds) {
        ritHz = 0;
        if (ritActive) dds.setFrequencyWithRIT(vfoHz, 0);
        dirtyFreq = true;
    }

    // ── Lock ─────────────────────────────────────────────────
    void toggleLock() {
        locked = !locked;
        dirtyBotBar = true;
        dirtyFull   = true;
    }

    // ── Mode (from TR7 mode switch) ───────────────────────────
    void setMode(uint8_t idx) {
        if (idx >= NUM_MODES || idx == modeIdx) return;
        modeIdx = idx;
        dirtyFull = true;
    }

    // ── Memory ───────────────────────────────────────────────
    void recallMemory(uint8_t bank, uint8_t slot,
                      MemoryManager& mem, AD9851& dds) {
        const MemSlot* m = mem.get(bank, slot);
        if (!m || !m->valid) return;

        bandIdx   = m->bandIdx;
        vfoHz     = m->vfoHz;
        stepHz    = m->stepHz;
        stepIdx   = 0;
        for (int i = 0; i < NUM_STEPS; i++) {
            if (STEP_RATES[i] == stepHz) { stepIdx = i; break; }
        }
        modeIdx   = m->modeIdx;
        memBank   = bank;
        activeMem = slot;
        memDirty  = false;
        ritHz     = 0;

        dds.setFrequency(vfoHz);
        dirtyFull  = true;
        dirtyFreq  = true;
        dirtyBotBar = true;
    }

    void saveToMemory(uint8_t bank, uint8_t slot,
                      MemoryManager& mem) {
        const BandDef& b = BANDS[bandIdx];
        char label[MEM_LABEL_LEN + 1];
        MemoryManager::autoLabel(label, b.name, dialMHz());

        mem.save(bank, slot,
                 vfoHz, bandIdx, modeIdx, stepHz,
                 label, MODE_NAMES[modeIdx]);

        memBank   = bank;
        activeMem = slot;
        memDirty  = false;
        dirtyFull = true;
    }

    // Memory label for top bar badge ("A3", "B0" etc)
    void memLabel(char* buf) const {
        if (activeMem < 0 || memDirty) { buf[0] = 0; return; }
        snprintf(buf, 4, "%c%d", 'A' + memBank, activeMem);
    }
};
