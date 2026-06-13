// ============================================================
//  memory.h — Programmable memory channels
//  10 banks (A–J) × 10 slots = 100 memories
//  Stored in ESP32 NVS flash via Preferences library
//  Survives power cycles, handles wear levelling automatically
// ============================================================
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

struct MemSlot {
    bool     valid;
    uint32_t vfoHz;
    uint8_t  bandIdx;
    uint8_t  modeIdx;
    uint32_t stepHz;
    char     label[MEM_LABEL_LEN + 1];
    char     mode[4];       // "USB" / "LSB" / "CW" / "AM" / "FM"
};

class MemoryManager {
public:
    MemSlot banks[MEM_BANKS][MEM_SLOTS_PER_BANK];

    void begin() {
        memset(banks, 0, sizeof(banks));
        load();
    }

    // Save current VFO state to a slot
    void save(uint8_t bank, uint8_t slot,
              uint32_t vfoHz, uint8_t bandIdx, uint8_t modeIdx,
              uint32_t stepHz, const char* label, const char* mode) {
        if (bank >= MEM_BANKS || slot >= MEM_SLOTS_PER_BANK) return;

        MemSlot& m = banks[bank][slot];
        m.valid    = true;
        m.vfoHz    = vfoHz;
        m.bandIdx  = bandIdx;
        m.modeIdx  = modeIdx;
        m.stepHz   = stepHz;
        strncpy(m.label, label, MEM_LABEL_LEN);
        m.label[MEM_LABEL_LEN] = 0;
        strncpy(m.mode, mode, 3);
        m.mode[3] = 0;

        persist(bank, slot);
    }

    // Clear a slot
    void clear(uint8_t bank, uint8_t slot) {
        if (bank >= MEM_BANKS || slot >= MEM_SLOTS_PER_BANK) return;
        banks[bank][slot].valid = false;
        persist(bank, slot);
    }

    bool isValid(uint8_t bank, uint8_t slot) const {
        if (bank >= MEM_BANKS || slot >= MEM_SLOTS_PER_BANK) return false;
        return banks[bank][slot].valid;
    }

    const MemSlot* get(uint8_t bank, uint8_t slot) const {
        if (bank >= MEM_BANKS || slot >= MEM_SLOTS_PER_BANK) return nullptr;
        return &banks[bank][slot];
    }

    // Return index of next filled slot in bank, wrapping around.
    // Returns -1 if bank is empty.
    int nextFilled(uint8_t bank, int currentSlot) const {
        if (bank >= MEM_BANKS) return -1;
        for (int i = 1; i <= MEM_SLOTS_PER_BANK; i++) {
            int s = (currentSlot + i) % MEM_SLOTS_PER_BANK;
            if (banks[bank][s].valid) return s;
        }
        return -1;
    }

    int countFilled(uint8_t bank) const {
        if (bank >= MEM_BANKS) return 0;
        int n = 0;
        for (int s = 0; s < MEM_SLOTS_PER_BANK; s++)
            if (banks[bank][s].valid) n++;
        return n;
    }

    // Auto-generate a label from band name + dial frequency
    static void autoLabel(char* buf, const char* bandName, float dialMHz) {
        snprintf(buf, MEM_LABEL_LEN + 1, "%s%.2f", bandName, dialMHz);
    }

private:
    Preferences prefs;

    // NVS key format: "bXsY" where X=bank 0-9, Y=slot 0-9
    void keyFor(char* key, uint8_t bank, uint8_t slot) {
        snprintf(key, 8, "b%ds%d", bank, slot);
    }

    void persist(uint8_t bank, uint8_t slot) {
        char key[8];
        keyFor(key, bank, slot);
        const MemSlot& m = banks[bank][slot];

        prefs.begin(MEM_NAMESPACE, false);
        if (m.valid) {
            // Pack into a compact byte array to save NVS space
            // Format: [valid(1)][vfoHz(4)][bandIdx(1)][modeIdx(1)]
            //         [stepHz(4)][label(9)][mode(4)] = 24 bytes
            uint8_t buf[24];
            buf[0] = 1;
            memcpy(buf+1,  &m.vfoHz,   4);
            buf[5] = m.bandIdx;
            buf[6] = m.modeIdx;
            memcpy(buf+7,  &m.stepHz,  4);
            memcpy(buf+11, m.label,    MEM_LABEL_LEN+1);
            memcpy(buf+20, m.mode,     4);
            prefs.putBytes(key, buf, sizeof(buf));
        } else {
            prefs.remove(key);
        }
        prefs.end();
    }

    void load() {
        prefs.begin(MEM_NAMESPACE, true);   // read-only
        for (uint8_t b = 0; b < MEM_BANKS; b++) {
            for (uint8_t s = 0; s < MEM_SLOTS_PER_BANK; s++) {
                char key[8];
                keyFor(key, b, s);
                uint8_t buf[24] = {0};
                size_t  len = prefs.getBytes(key, buf, sizeof(buf));
                if (len == sizeof(buf) && buf[0] == 1) {
                    MemSlot& m = banks[b][s];
                    m.valid = true;
                    memcpy(&m.vfoHz,  buf+1,  4);
                    m.bandIdx  = buf[5];
                    m.modeIdx  = buf[6];
                    memcpy(&m.stepHz, buf+7,  4);
                    memcpy(m.label,   buf+11, MEM_LABEL_LEN+1);
                    memcpy(m.mode,    buf+20, 4);
                }
            }
        }
        prefs.end();
    }
};
