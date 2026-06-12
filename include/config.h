// ============================================================
//  config.h — TR7 DDS VFO hardware configuration
//  All pin assignments and tunable constants live here.
//  Edit this file for your specific wiring, not the source.
// ============================================================
#pragma once

// ------------------------------------------------------------
//  AD9851 DDS connections (bit-bang — any GPIO will do)
// ------------------------------------------------------------
#define DDS_CLK     25          // Serial clock
#define DDS_DATA    26          // Serial data
#define DDS_FQ_UD   27          // Frequency update (rising edge loads FTW)
#define DDS_RESET   32          // Active high reset

// AD9851 reference clock
#define DDS_REFCLK_HZ   25000000UL   // 25 MHz TCXO
#define DDS_MULTIPLIER  6            // On-chip 6x PLL → 180 MHz system clock
#define DDS_SYSCLK_HZ   (DDS_REFCLK_HZ * DDS_MULTIPLIER)

// VFO output range — TR7 PTO window
#define VFO_MIN_HZ      5000000UL
#define VFO_MAX_HZ      5500000UL

// ------------------------------------------------------------
//  Rotary encoder
//  Use interrupt-capable pins. On ESP32 any GPIO can interrupt.
//  Recommended: avoid GPIO 34-39 (input-only, no pull-up).
// ------------------------------------------------------------
#define ENC_A       34          // Encoder A — use 10k external pull-up
#define ENC_B       35          // Encoder B — use 10k external pull-up
#define ENC_BTN     0           // Encoder push button (also BOOT button — convenient)

// Encoder behaviour
#define ENC_STEPS_PER_DETENT  4     // Most encoders: 4 pulses per click
#define ENC_ACCEL_THRESHOLD   8     // Ticks/100ms above which acceleration kicks in
#define ENC_ACCEL_FACTOR      5     // Multiplier when accelerating

// ------------------------------------------------------------
//  TR7 mode tracking
//  The TR7 mode switch grounds one of these lines.
//  All five need 10k pull-ups to 3.3V.
//  Use input-only pins (34-39) to save interrupt pins.
// ------------------------------------------------------------
#define MODE_USB    36
#define MODE_LSB    39
#define MODE_CW     34          // Share with ENC_A if short of pins — poll only
#define MODE_AM     35          // Share with ENC_B if short of pins — poll only
#define MODE_FM     -1          // Set to -1 if TR7 does not have FM

// Mode poll interval (ms) — mode switch doesn't need fast response
#define MODE_POLL_MS    200

// ------------------------------------------------------------
//  Display (TFT_eSPI — pins set via User_Setup.h / build flags)
//  Defined here for reference only — do not redefine.
// ------------------------------------------------------------
// TFT_MOSI  13   TFT_SCLK  14   TFT_MISO  12
// TFT_CS    15   TFT_DC     2   TFT_RST   -1
// TFT_BL    21   TOUCH_CS  33   TOUCH_IRQ 36

// Display update intervals (ms)
#define DISPLAY_FREQ_MS     50   // Frequency/VFO zone — fast, uses sprite
#define DISPLAY_SMETER_MS   80   // S-meter animation tick
#define DISPLAY_FULL_MS    500   // Full redraw (band zone, buttons etc)

// ------------------------------------------------------------
//  Memory storage (NVS via Preferences library)
// ------------------------------------------------------------
#define MEM_BANKS           10
#define MEM_SLOTS_PER_BANK  10
#define MEM_LABEL_LEN        8
#define MEM_NAMESPACE       "tr7vfo"     // NVS namespace
#define MEM_LONG_PRESS_MS   800          // Hold time to save memory

// ------------------------------------------------------------
//  RIT
// ------------------------------------------------------------
#define RIT_MAX_HZ      1000    // ±1 kHz
#define RIT_STEP_HZ       10   // 10 Hz per encoder tick while RIT active

// ------------------------------------------------------------
//  Scan
// ------------------------------------------------------------
#define SCAN_DWELL_MS   1500   // Time on each memory before stepping

// ------------------------------------------------------------
//  Step rates (Hz) — index into stepRates[] array
// ------------------------------------------------------------
#define NUM_STEPS   6
const uint32_t STEP_RATES[NUM_STEPS] = {
    1, 10, 100, 1000, 10000, 100000
};
#define DEFAULT_STEP_IDX    3   // 1 kHz default

// ------------------------------------------------------------
//  Band definitions
// ------------------------------------------------------------
#define NUM_BANDS   11
#define CB_BAND_IDX  8
#define MAR_BAND_IDX 10

struct BandDef {
    const char* name;
    uint32_t    vfoMin;     // Hz — VFO range for this band
    uint32_t    vfoMax;
    int32_t     offset;     // Hz — dial = VFO + offset
    bool        isCB;
    bool        isMarine;
    uint32_t    defaultVFO; // Starting frequency when band selected
};

const BandDef BANDS[NUM_BANDS] = {
//   name    vfoMin    vfoMax    offset       CB     Mar   defaultVFO
    {"160m", 5000000, 5500000, -3100000L,   false, false, 5100000},  // 1.9 MHz
    {"80m",  5000000, 5500000, -1500000L,   false, false, 5075000},  // 3.575 MHz
    {"40m",  5000000, 5500000,  2000000L,   false, false, 5100000},  // 7.100 MHz
    {"30m",  5000000, 5500000,  5100000L,   false, false, 5110000},  // 10.210 MHz
    {"20m",  5000000, 5500000,  9000000L,   false, false, 5100000},  // 14.100 MHz
    {"17m",  5000000, 5500000, 12800000L,   false, false, 5068000},  // 17.868 MHz
    {"15m",  5000000, 5500000, 16000000L,   false, false, 5100000},  // 21.100 MHz
    {"12m",  5000000, 5500000, 19700000L,   false, false, 5050000},  // 24.950 MHz
    {"11m",  5030000, 5470000, 21935000L,   true,  false, 5250000},  // 27.185 MHz Ch19
    {"10m",  5000000, 5500000, 24500000L,   false, false, 5100000},  // 29.600 MHz
    {"Mar",  5070000, 5210000, 20935000L,   false, true,  5143000},  // 26.078 MHz
};

// ------------------------------------------------------------
//  CB channel tables (120 channels: Lower 40 + Std 40 + Upper 40)
// ------------------------------------------------------------
#define CB_TOTAL_CHANNELS  120
#define CB_STD_OFFSET       40   // Standard 40 start index in ALL_CB[]

// Lower 40: 26.515–26.955 MHz (10 kHz spacing)
// Standard 40: 26.965–27.405 MHz (non-uniform — see array)
// Upper 40: 27.415–27.815 MHz (10 kHz spacing — note: not 27.855)
// Note: upper limit varies by region. Adjust UPPER_MAX if needed.

// Marine HF channels (subset — 25–26 MHz allocation)
#define MAR_TOTAL_CHANNELS  22
