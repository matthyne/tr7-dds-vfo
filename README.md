# TR7 DDS VFO

Replacement VFO firmware for the Ten-Tec TR7 HF transceiver. Runs on an ESP32-2432S028 ("Cheap Yellow Display") and drives an AD9851 DDS chip to replace the TR7's original PTO with a stable, touchscreen-controlled oscillator.

## Features

- **Touchscreen frequency display** — large 7-segment style dial readout, VFO sub-line, FTW diagnostic
- **11 bands** — 160m through 10m plus CB (120-channel) and HF Marine (22-channel)
- **CB channel snap** — L1–L40, S1–S40 (with S19★ calling channel), U1–U40
- **6 tuning steps** — 1 Hz to 100 kHz, selectable by touch or encoder button
- **Encoder acceleration** — fast spin multiplies step by 5×
- **RIT** — ±1 kHz in 10 Hz steps; encoder long-press clears offset
- **100 programmable memories** — 10 banks × 10 slots, persisted to NVS flash
- **Memory scan** — steps through filled slots in the active bank at 1.5 s dwell
- **TR7 mode tracking** — reads USB/LSB/CW/AM/FM switch lines and displays current mode
- **S-meter** — 12-segment bar (simulated; hookup point documented for real AGC)

## Hardware

| Item | Part |
|------|------|
| MCU / display | ESP32-2432S028 "Cheap Yellow Display" |
| DDS | AD9851 module (25 MHz TCXO, 6× PLL = 180 MHz sysclk) |
| Encoder | Any quadrature encoder with push button |

### Wiring (defaults in `include/config.h`)

**AD9851 DDS**

| Signal | GPIO |
|--------|------|
| CLK | 25 |
| DATA | 26 |
| FQ_UD | 27 |
| RESET | 32 |

**Rotary encoder** (10 kΩ external pull-ups on A/B)

| Signal | GPIO |
|--------|------|
| A | 34 |
| B | 35 |
| Button | 0 (boot button) |

**TR7 mode switch** (5 lines, grounded by mode switch, 10 kΩ pull-ups to 3.3 V)

| Mode | GPIO |
|------|------|
| USB | 36 |
| LSB | 39 |
| CW | 34 |
| AM | 35 |
| FM | — (set `MODE_FM -1` if not fitted) |

The CYD display/touch pins are fixed hardware and are configured automatically via build flags.

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
cd tr7-dds-vfo
pio run                     # build main firmware
pio run -t upload           # flash main firmware
pio device monitor          # 115200 baud serial console
```

TFT_eSPI is configured entirely via build flags in `platformio.ini` — no library files need editing.

### Touch calibration

Build and flash the calibration sketch with `pio run -e touch_calibrate -t upload`, read the five values from serial output, and update `TOUCH_CAL_DATA` in `include/display.h`. The defaults are approximate for the CYD.

### Display/touch smoke test

`pio run -e display_test -t upload` flashes a standalone sketch that exercises the display, fonts, sprites, and touch — useful for checking wiring and `TFT_eSPI` config before building the full firmware.

## Configuration

All pin assignments and tunable constants are in `include/config.h`. Edit this file for your wiring — do not edit the source modules.

Key constants:

| Constant | Default | Description |
|----------|---------|-------------|
| `DDS_REFCLK_HZ` | 25 000 000 | TCXO frequency |
| `DDS_MULTIPLIER` | 6 | On-chip PLL multiplier |
| `VFO_MIN_HZ` / `VFO_MAX_HZ` | 5.0–5.5 MHz | TR7 PTO window |
| `RIT_MAX_HZ` | 1000 | Max RIT offset |
| `SCAN_DWELL_MS` | 1500 | Memory scan dwell time |
| `MEM_LONG_PRESS_MS` | 800 | Long-press threshold for memory save |

## Display Layout

```
┌──────────────────────────────────┐
│ USB  RIT  LOCK  [mem badge]  EXT │  top bar
├──────────────────────────────────┤
│          14.100 MHz              │  large dial freq
│  VFO 5.100000 MHz  RIT +0.50kHz │
│  FTW 0x0A3D70A4                  │
├──────────────────────────────────┤
│ 1Hz 10Hz 100Hz 1kHz 10kHz 100kHz│  step row
├──────────────────────────────────┤
│ 160m 80m 40m 30m 20m 17m        │  band zone
│ 15m  12m 11m 10m Mar            │
├──────────────────────────────────┤
│ A  B  C  D  E  F  G  H  I  J   │  memory bank tabs
│ [0][1][2][3][4][5][6][7][8][9] │  memory cells
├──────────────────────────────────┤
│ RIT  LOCK  SCAN  M-SAVE  ●●●●● │  bottom bar + S-meter
└──────────────────────────────────┘
```

## Touch Controls

| Area | Short tap | Long press |
|------|-----------|------------|
| Step row | Select step rate | — |
| Band buttons | Select band | — |
| Memory bank tabs | Switch bank | — |
| Memory cell (filled) | Recall | Overwrite save |
| Memory cell (empty) | Save current freq | — |
| RIT button | Toggle RIT | — |
| LOCK button | Toggle lock | — |
| SCAN button | Toggle scan | — |
| M-SAVE button | Save to first empty slot | — |
| Mode pill (top-left) | Cycle mode | — |

## Encoder Controls

| Action | Effect |
|--------|--------|
| Rotate | Tune VFO (or RIT offset if RIT active) |
| Short press | Step rate up (cycles 1 Hz → 100 kHz → wrap) |
| Long press (≥800 ms) | Clear RIT offset |

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for a full technical description of the firmware modules, concurrency model, frequency arithmetic, display refresh strategy, and NVS memory format.
