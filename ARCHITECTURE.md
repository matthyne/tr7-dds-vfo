# TR7 DDS VFO — Firmware Architecture

## Overview

ESP32-based DDS VFO replacement for the Ten-Tec TR7 HF transceiver. The firmware drives an AD9851 DDS chip via bit-bang serial, producing a clean 5.0–5.5 MHz signal that replaces the TR7's original PTO (Permeability-Tuned Oscillator). A touchscreen display on the ESP32-2432S028 ("Cheap Yellow Display") provides frequency readout, band selection, memory recall, and function controls.

## Hardware Platform

| Component | Part | Interface |
|-----------|------|-----------|
| MCU | ESP32-2432S028 ("CYD") | — |
| Display | ILI9341 320×240 TFT | VSPI SPI @ 40 MHz |
| Touch | XPT2046 resistive | VSPI SPI @ 2.5 MHz (shared bus, TOUCH_CS=33) |
| DDS | AD9851 | Bit-bang serial (GPIO 25/26/27/32) |
| Encoder | Quadrature + push button | GPIO 34/35/0 |
| TR7 mode switch | 5-line grounded bus | GPIO 36/39/34/35 |

### SPI bus sharing

The ILI9341 display and XPT2046 touch controller share the VSPI bus. `SUPPORT_TRANSACTIONS` is enabled in TFT_eSPI to arbitrate correctly. DMA (`ESP32_DMA`) offloads display writes from the CPU.

### AD9851 timing

- Reference clock: 25 MHz TCXO
- On-chip 6× PLL → 180 MHz system clock
- Frequency tuning word (FTW): 32-bit, MSB first then 8-bit control byte (0x01 = 6× enable)
- Resolution: `180 MHz / 2^32` ≈ 0.0419 Hz/LSB
- VFO output range constrained to 5.000–5.500 MHz to match TR7 PTO window

## Module Map

```
include/
  config.h      — Pin assignments, timing constants, band table, step rates
  bands.h       — CB (120-channel) and HF Marine (22-channel) frequency tables
  dds.h         — AD9851 bit-bang driver
  memory.h      — NVS memory manager (10 banks × 10 slots)
  vfo_state.h   — Central state struct + all state-transition methods
  display.h     — TFT_eSPI wrapper, colour palette, layout zones, sprite rendering
  touch_ui.h    — Touch zone hit-testing and TouchEvent generation
src/
  main.cpp      — Arduino setup/loop, FreeRTOS mutex, encoder ISR, scan, mode polling
```

## Concurrency Model

The ESP32 is dual-core. FreeRTOS is used minimally:

- **Core 1 (setup / Arduino default)**: DDS writes, encoder ISR (via ESP32Encoder PCNT hardware)
- **Core 0 (loop)**: display updates, touch processing, memory scan, mode polling

A single `SemaphoreHandle_t stateMutex` (`TAKE_STATE` / `GIVE_STATE` macros) protects all accesses to `VFOState`. Every handler in `loop()` that reads or writes state holds the mutex for the minimum required duration.

## State Machine — `VFOState`

All mutable state lives in a single `VFOState` struct (`vfo_state.h`). No global variables are modified directly from outside the struct. All transitions are methods:

| Method | Effect |
|--------|--------|
| `tune(ticks, dds)` | Move VFO by `ticks × stepHz`; clamps to band limits; RIT-aware |
| `setBand(idx, dds)` | Jump to band default VFO; forces 10 kHz step on CB band |
| `toggleRIT(dds)` | Enable/disable RIT; clearing resets offset and re-drives DDS |
| `clearRIT(dds)` | Zero RIT offset without toggling active flag |
| `toggleLock()` | Freeze/unfreeze encoder tuning |
| `setMode(idx)` | Update mode index (from TR7 switch polling or touch) |
| `recallMemory(bank, slot, mem, dds)` | Restore frequency/band/mode/step from NVS slot |
| `saveToMemory(bank, slot, mem)` | Persist current state to NVS slot with auto-label |

### Frequency arithmetic

```
dialHz  = vfoHz + BANDS[bandIdx].offset    // what the operator tunes to
effectiveVFO = vfoHz + (ritActive ? ritHz : 0)   // what the DDS outputs
```

RIT range: ±1000 Hz in 10 Hz steps. The DDS always receives the effective (RIT-applied) frequency; `vfoHz` stores the base without offset.

### Dirty flags

Three boolean dirty flags on `VFOState` drive selective display refresh without polling:

| Flag | Triggers |
|------|----------|
| `dirtyFreq` | Any frequency change (tune, RIT, recall) |
| `dirtyFull` | Band change, step change, memory operation, lock, mode |
| `dirtyBotBar` | RIT toggle, lock toggle, scan toggle |

## Display Architecture

Layout is fixed landscape 320×240:

```
┌─────────────────────────────────┐ y=0
│  Top bar: mode pill / badges    │ h=24
├─────────────────────────────────┤ y=24
│  Frequency zone (sprite)        │ h=60
│    Large dial freq (Font 7)     │
│    VFO sub-line / RIT offset    │
│    FTW sub-line                 │
├─────────────────────────────────┤ y=84
│  RIT bar (visible when RIT on)  │ h=18
├─────────────────────────────────┤ y=102
│  Step row  [1Hz…100kHz]×6      │ h=22
├─────────────────────────────────┤ y=124
│  Band zone [160m…Mar]×11       │ h=26
├─────────────────────────────────┤ y=150
│  Memory zone: bank tabs A–J    │
│              cells 0–9         │ h=52 (total)
├─────────────────────────────────┤ y=210
│  Bottom bar: RIT/LOCK/SCAN/M-SAVE + S-meter │ h=30
└─────────────────────────────────┘ y=240
```

Two `TFT_eSprite` objects eliminate flicker:
- `sprFreq` — frequency zone (redrawn at up to 20 Hz)
- `sprStatus` — top bar (redrawn with frequency zone)

The static chrome (divider lines, labels) is drawn once at `begin()` and never redrawn unless `dirtyFull` fires.

### Colour palette (RGB565 dark radio theme)

Key named colours: `C_TEAL` (ham active), `C_AMBER` (RIT), `C_RED` (lock), `C_PURPLE` (CB/memory), `C_BLUE` (marine).

### Update rates

| Zone | Rate |
|------|------|
| Frequency / top bar | max 20 Hz (`DISPLAY_FREQ_MS=50`) or on `dirtyFreq` |
| S-meter | ~12.5 Hz (`DISPLAY_SMETER_MS=80`) |
| Full redraw | max 2 Hz (`DISPLAY_FULL_MS=500`) or on `dirtyFull` |

## Touch UI

`TouchUI::process()` translates raw XPT2046 coordinates to typed `TouchEvent` values. Debounce is 120 ms. Memory cells use a press-timer to distinguish tap (recall) from long press ≥800 ms (save).

| Action | Zone |
|--------|------|
| `StepSelect` | Step row, column 0–5 |
| `BandSelect` | Band zone, 2 rows of 6 |
| `MemTabSelect` | Memory bank tabs A–J |
| `MemCellTap` | Memory cell (short tap = recall; empty cell = save) |
| `MemCellLong` | Memory cell (long press = overwrite save) |
| `BtnRIT/Lock/Scan/MemSave` | Bottom bar buttons |
| `ModeToggle` | Left ~60px of top bar |

## Memory System

100 slots: 10 banks (A–J) × 10 slots each. Stored in ESP32 NVS flash via the Arduino `Preferences` library (namespace `tr7vfo`). Each slot is packed into 24 bytes:

```
[valid(1)][vfoHz(4)][bandIdx(1)][modeIdx(1)][stepHz(4)][label(9)][mode(4)]
```

NVS key format: `bXsY` (e.g., `b0s3` = bank 0 slot 3).

## Band Table

11 bands defined in `config.h`. Each entry provides:
- `vfoMin/vfoMax` — VFO hardware range (all within 5.0–5.5 MHz)
- `offset` — signed Hz added to VFO to produce dial frequency
- `isCB` / `isMarine` — changes UI colour theme and channel snap-to behaviour
- `defaultVFO` — starting VFO when band is selected

CB band (11m) constrains VFO to 5.030–5.470 MHz and forces 10 kHz step. Marine band covers HF ship channels 25–26 MHz.

## Encoder

ESP32Encoder library uses hardware PCNT peripheral for quadrature decoding. `ENC_STEPS_PER_DETENT=4` normalises count to detents. Acceleration: if ticks per 100 ms window exceeds `ENC_ACCEL_THRESHOLD=8`, the step is multiplied by `ENC_ACCEL_FACTOR=5`.

Encoder button: short press cycles step rate up through `STEP_RATES[]`; long press (≥800 ms) clears RIT offset.

## Build System

PlatformIO, `espressif32` platform, `arduino` framework. TFT_eSPI is configured entirely via `-D` build flags in `platformio.ini` — the library's `User_Setup.h` is not edited directly. `User_Setup.h` in the project root documents the same settings for reference/Arduino IDE use.

Partition scheme: `min_spiffs.csv` reserves SPIFFS for smooth fonts.
