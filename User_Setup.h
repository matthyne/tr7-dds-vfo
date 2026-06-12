// ============================================================
//  TFT_eSPI User_Setup.h
//  Target: ESP32-2432S028 ("Cheap Yellow Display")
//  Display: ILI9341  320x240  SPI
//  Touch:   XPT2046  resistive  SPI (shared bus, separate CS)
//
//  INSTALLATION
//  ------------
//  Copy this file to:
//    Arduino:      ~/Arduino/libraries/TFT_eSPI/User_Setup.h
//    PlatformIO:   defined via build_flags in platformio.ini
//                  (see platformio.ini in this project)
//
//  Do NOT edit the library's own User_Setup_Select.h —
//  just replace User_Setup.h with this file.
// ============================================================

// ------------------------------------------------------------
//  Driver selection
// ------------------------------------------------------------
#define ILI9341_DRIVER          // 320x240 panel on CYD

// ------------------------------------------------------------
//  Display dimensions
// ------------------------------------------------------------
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

// ------------------------------------------------------------
//  SPI pins — CYD hardwired values
//  The CYD uses the ESP32 VSPI bus for the display.
//  Do not change these unless you have modified the hardware.
// ------------------------------------------------------------
#define TFT_MOSI    13          // SPI data out   (VSPI MOSI)
#define TFT_SCLK    14          // SPI clock      (VSPI CLK)
#define TFT_MISO    12          // SPI data in    (VSPI MISO) — touch only
#define TFT_CS      15          // Display chip select
#define TFT_DC       2          // Data / Command
#define TFT_RST     -1          // Reset — tied to EN on CYD, use -1

// Backlight — CYD drives BL via GPIO 21 (active high, PWM capable)
#define TFT_BL      21
#define TFT_BACKLIGHT_ON HIGH

// ------------------------------------------------------------
//  Touch controller — XPT2046 shares VSPI bus
// ------------------------------------------------------------
#define TOUCH_CS    33          // XPT2046 chip select (separate from display CS)
// Touch IRQ is on GPIO 36 — handled in firmware, not here

// ------------------------------------------------------------
//  SPI frequency
//  Display: 55 MHz is the ILI9341 maximum; 40 MHz is safer
//           for the PCB trace lengths on the CYD.
//  Touch:   XPT2046 maximum is 2.5 MHz — keep this low.
// ------------------------------------------------------------
#define SPI_FREQUENCY           40000000
#define SPI_READ_FREQUENCY       20000000
#define SPI_TOUCH_FREQUENCY       2500000

// ------------------------------------------------------------
//  Colour depth
//  16-bit (65536 colours) is correct for ILI9341.
//  Do not change to 8-bit — it breaks the colour mapping.
// ------------------------------------------------------------
#define COLOR_DEPTH 16

// ------------------------------------------------------------
//  Font loading
//  Fonts 2, 4, 6, 7, 8 are built into TFT_eSPI.
//  Font 6 is a large 7-segment style — good for the freq display.
//  Font 7 is similar but with decimal point support.
//  Font 8 is the largest — 75px digits.
// ------------------------------------------------------------
#define LOAD_GLCD               // Font 1 — small default
#define LOAD_FONT2              // Font 2 — small proportional
#define LOAD_FONT4              // Font 4 — medium proportional
#define LOAD_FONT6              // Font 6 — large 7-seg digits
#define LOAD_FONT7              // Font 7 — 7-seg with decimal point
#define LOAD_FONT8              // Font 8 — large 7-seg digits
#define LOAD_GFXFF              // FreeFonts — needed for custom fonts
#define SMOOTH_FONT             // Enable smooth (anti-aliased) fonts via SPIFFS

// ------------------------------------------------------------
//  DMA transfers (optional but recommended)
//  Enables background SPI DMA so the CPU is free during
//  display writes. Gives noticeably smoother updates.
// ------------------------------------------------------------
#define ESP32_DMA
#define SUPPORT_TRANSACTIONS    // Required for DMA + shared SPI bus

// ------------------------------------------------------------
//  Rotation
//  0 = portrait  (240 wide, 320 tall)
//  1 = landscape (320 wide, 240 tall) — use this for VFO display
//  2 = portrait inverted
//  3 = landscape inverted
//  Set in firmware with tft.setRotation(1) — this is just default.
// ------------------------------------------------------------
// #define TFT_ROTATION 1       // Set in firmware instead

// ------------------------------------------------------------
//  Sanity check — will produce a compile error if pins conflict
// ------------------------------------------------------------
#if (TFT_CS == TOUCH_CS)
  #error "TFT_CS and TOUCH_CS must be different pins"
#endif
