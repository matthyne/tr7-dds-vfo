// ============================================================
//  dds.h — AD9851 DDS driver
//  Bit-bang serial interface (W4 MSB first, then control byte)
// ============================================================
#pragma once
#include <Arduino.h>
#include "config.h"

class AD9851 {
public:
    void begin() {
        pinMode(DDS_CLK,   OUTPUT);
        pinMode(DDS_DATA,  OUTPUT);
        pinMode(DDS_FQ_UD, OUTPUT);
        pinMode(DDS_RESET, OUTPUT);

        digitalWrite(DDS_CLK,   LOW);
        digitalWrite(DDS_DATA,  LOW);
        digitalWrite(DDS_FQ_UD, LOW);

        reset();
    }

    void reset() {
        // AD9851 reset sequence: pulse RESET high, then pulse FQ_UD
        digitalWrite(DDS_RESET, HIGH); delayMicroseconds(5);
        digitalWrite(DDS_RESET, LOW);  delayMicroseconds(5);
        // Switch to serial mode: pulse W_CLK then FQ_UD
        pulse(DDS_CLK);
        pulse(DDS_FQ_UD);
    }

    // Set output frequency in Hz
    void setFrequency(uint32_t freqHz) {
        uint32_t ftw = calcFTW(freqHz);
        loadFTW(ftw);
        _currentHz  = freqHz;
        _currentFTW = ftw;
    }

    // Apply a frequency + RIT offset atomically
    void setFrequencyWithRIT(uint32_t freqHz, int32_t ritHz) {
        int32_t effective = (int32_t)freqHz + ritHz;
        if (effective < (int32_t)VFO_MIN_HZ) effective = VFO_MIN_HZ;
        if (effective > (int32_t)VFO_MAX_HZ) effective = VFO_MAX_HZ;
        uint32_t ftw = calcFTW((uint32_t)effective);
        loadFTW(ftw);
        _currentHz  = freqHz;    // stored without RIT offset
        _currentFTW = ftw;       // stored with RIT offset applied
    }

    uint32_t currentHz()  const { return _currentHz; }
    uint32_t currentFTW() const { return _currentFTW; }

    // Static helper — useful for display calculations
    static uint32_t calcFTW(uint32_t freqHz) {
        // FTW = (freq × 2^32) / sysclk
        // Must use 64-bit intermediate to avoid overflow
        return (uint32_t)(((uint64_t)freqHz << 32) / DDS_SYSCLK_HZ);
    }

    static float resolution() {
        return (float)DDS_SYSCLK_HZ / 4294967296.0f;   // Hz per LSB
    }

private:
    uint32_t _currentHz  = 0;
    uint32_t _currentFTW = 0;

    void pulse(uint8_t pin) {
        digitalWrite(pin, HIGH);
        digitalWrite(pin, LOW);
    }

    // Load a 32-bit FTW + control byte into AD9851
    // Serial mode: W4 (MSB) first, W1 last, then 8-bit control byte
    // Control byte 0x01: enable 6x multiplier
    void loadFTW(uint32_t ftw) {
        digitalWrite(DDS_FQ_UD, LOW);

        // Send 32 FTW bits MSB first
        for (int b = 31; b >= 0; b--) {
            digitalWrite(DDS_DATA, (ftw >> b) & 1);
            digitalWrite(DDS_CLK, HIGH);
            digitalWrite(DDS_CLK, LOW);
        }

        // Send control byte: bit0 = 1 enables 6x multiplier
        // Bits 7:5 = phase word (0 = 0° phase)
        uint8_t ctrl = 0x01;
        for (int b = 7; b >= 0; b--) {
            digitalWrite(DDS_DATA, (ctrl >> b) & 1);
            digitalWrite(DDS_CLK, HIGH);
            digitalWrite(DDS_CLK, LOW);
        }

        // Pulse FQ_UD to transfer shadow register → active register
        digitalWrite(DDS_FQ_UD, HIGH);
        digitalWrite(DDS_FQ_UD, LOW);
    }
};
