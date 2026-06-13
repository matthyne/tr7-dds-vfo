# CLAUDE.md — TR7 DDS VFO

## Project

ESP32 firmware replacing the VFO/PTO of a Drake TR-7 HF transceiver. Target board is the ESP32-2432S028 "Cheap Yellow Display" (CYD). See README.md for user-facing docs and ARCHITECTURE.md for full technical detail.

## Key facts

- DDS covers 5.0–5.5 MHz (TR-7 PTO range)
- 11 bands including 11m CB (120 channels) and HF Marine (22 channels)
- 10 banks × 10 memory channels stored in NVS
- Display: ILI9341 320×240 via TFT_eSPI
- Touch: XPT2046 resistive, shared SPI bus

## Build system

PlatformIO (`platformio.ini`). Default environment `tr7_vfo` builds `src/main.cpp` only — `build_src_filter` excludes the diagnostic sketches below, since every `.ino`/`.cpp` in `src/` would otherwise be compiled into one binary (multiple `setup()`/`loop()` → link error). TFT_eSPI is configured entirely via `-D` build flags — do **not** edit the library's own `User_Setup.h`. `User_Setup.h` at the project root is reference/Arduino IDE documentation only.

```bash
pio run                              # build main firmware (tr7_vfo)
pio run -t upload                    # flash main firmware
pio device monitor                   # serial console 115200

pio run -e display_test -t upload    # flash display/touch smoke test
pio run -e touch_calibrate -t upload # flash touch calibration sketch
```

## Module layout

| File | Purpose |
|------|---------|
| `include/config.h` | All pin assignments and constants — edit here for wiring changes |
| `include/bands.h` | CB (120-channel) and HF Marine (22-channel) frequency tables |
| `include/dds.h` | AD9851 bit-bang driver |
| `include/memory.h` | NVS memory manager (10 banks × 10 slots); canonical `MemSlot` definition |
| `include/vfo_state.h` | Central state struct; all transitions are methods |
| `include/display.h` | TFT_eSPI wrapper; colour palette; sprite rendering |
| `include/touch_ui.h` | Touch zone hit-testing; TouchEvent enum |
| `src/main.cpp` | Arduino setup/loop; FreeRTOS mutex; encoder; scan; mode polling |
| `src/display_test.ino` | Display/touch smoke test (`env:display_test`, not part of main build) |
| `src/touch_calibrate.ino` | Touch calibration sketch (`env:touch_calibrate`, not part of main build) |

## Key design constraints

- **All mutable state in `VFOState`** — no globals modified outside the struct.
- **FreeRTOS mutex** (`stateMutex`) required for any read or write of `VFOState` from `loop()`.
- **Dirty flags** (`dirtyFreq`, `dirtyFull`, `dirtyBotBar`) drive display refresh — set them on state changes; do not call display methods directly from state transitions.
- **VFO range 5.0–5.5 MHz** — hard constraint matching TR-7 PTO window; never output outside this range.
- **AD9851 serial protocol** — W4 (MSB) first, then W1, then 8-bit control byte 0x01 (6× PLL). FTW = `(freq × 2^32) / 180_000_000`.
- **TFT_eSPI pin config** lives in `platformio.ini` build flags, mirrored in `User_Setup.h` for reference.
- **NVS key format** `bXsY` (e.g., `b0s3`), packed 24-byte struct per slot.

## Common tasks

**Change a pin** → edit `include/config.h` only.

**Add a band** → add entry to `BANDS[]` in `config.h`, increment `NUM_BANDS`, update display band labels in `display.h::drawBandZone()`.

**Add a CB/Marine channel** → edit the frequency arrays in `include/bands.h`.

**Wire real S-meter AGC** → replace `readSMeter()` body in `main.cpp` with `return analogRead(SMETER_PIN) / 341.0f;`.

**Touch calibration** → run `pio run -e touch_calibrate -t upload`, paste 5 values into `TOUCH_CAL_DATA` in `include/display.h`.
