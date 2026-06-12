// ============================================================
//  bands.h — Channel frequency tables
//  CB 120-channel (Lower + Standard + Upper 40)
//  HF Marine channels (25–26 MHz)
// ============================================================
#pragma once
#include <stdint.h>

// ------------------------------------------------------------
//  CB channels — 120 total
//  Index 0–39:   Lower 40   (L1–L40)  26.515–26.955 MHz
//  Index 40–79:  Standard 40 (S1–S40) 26.965–27.405 MHz
//  Index 80–119: Upper 40   (U1–U40)  27.415–27.815 MHz
//
//  Standard 40 has non-uniform spacing around Ch23–25 due to
//  the 1977 expansion from the original 23-channel plan.
//  All other groups are strict 10 kHz spacing.
// ------------------------------------------------------------
const uint32_t ALL_CB[120] = {
    // Lower 40 — L1 to L40 (26.515–26.955 MHz, 10 kHz steps)
    26515000, 26525000, 26535000, 26545000, 26555000,
    26565000, 26575000, 26585000, 26595000, 26605000,
    26615000, 26625000, 26635000, 26645000, 26655000,
    26665000, 26675000, 26685000, 26695000, 26705000,
    26715000, 26725000, 26735000, 26745000, 26755000,
    26765000, 26775000, 26785000, 26795000, 26805000,
    26815000, 26825000, 26835000, 26845000, 26855000,
    26865000, 26875000, 26885000, 26895000, 26955000,  // L40 = 26.955

    // Standard 40 — S1 to S40 (26.965–27.405 MHz)
    // Ch23/24/25 are out of strict 10 kHz order (historical)
    26965000, 26975000, 26985000, 27005000, 27015000,  // S1–S5
    27025000, 27035000, 27055000, 27065000, 27075000,  // S6–S10
    27085000, 27105000, 27115000, 27125000, 27135000,  // S11–S15
    27155000, 27165000, 27175000, 27185000, 27205000,  // S16–S20  (S19=27.185 calling)
    27215000, 27225000, 27255000, 27235000, 27245000,  // S21–S25  (S23/24/25 non-linear)
    27265000, 27275000, 27285000, 27295000, 27305000,  // S26–S30
    27315000, 27325000, 27335000, 27345000, 27355000,  // S31–S35
    27365000, 27375000, 27385000, 27395000, 27405000,  // S36–S40

    // Upper 40 — U1 to U40 (27.415–27.815 MHz, 10 kHz steps)
    27415000, 27425000, 27435000, 27445000, 27455000,
    27465000, 27475000, 27485000, 27495000, 27505000,
    27515000, 27525000, 27535000, 27545000, 27555000,
    27565000, 27575000, 27585000, 27595000, 27605000,
    27615000, 27625000, 27635000, 27645000, 27655000,
    27665000, 27675000, 27685000, 27695000, 27705000,
    27715000, 27725000, 27735000, 27745000, 27755000,
    27765000, 27775000, 27785000, 27795000, 27815000,  // U40 = 27.815
};

// Human-readable channel label — result in buf (caller provides >=6 bytes)
// Returns pointer to buf for convenience.
// Format: "L1"–"L40", "S1"–"S40", "U1"–"U40"
// S19 (index 58) is the highway calling channel — gets a ★ marker
inline const char* cbChannelLabel(int idx, char* buf) {
    if (idx < 0 || idx >= 120) { buf[0]=0; return buf; }
    if      (idx < 40)  snprintf(buf, 6, "L%d",  idx + 1);
    else if (idx < 80)  snprintf(buf, 6, idx==58 ? "S%d*" : "S%d", idx - 39);
    else                snprintf(buf, 6, "U%d",  idx - 79);
    return buf;
}

// Find the closest CB channel within tolerance Hz. Returns index (0-119) or -1.
inline int cbFindChannel(uint32_t dialHz, uint32_t toleranceHz = 600) {
    for (int i = 0; i < 120; i++) {
        uint32_t diff = dialHz > ALL_CB[i] ? dialHz - ALL_CB[i] : ALL_CB[i] - dialHz;
        if (diff <= toleranceHz) return i;
    }
    return -1;
}

// ------------------------------------------------------------
//  HF Marine channels — 25–26 MHz allocation
//  These are the commonly used ship-to-shore HF working channels.
//  Channel numbering follows ITU/IARU usage for this band.
// ------------------------------------------------------------
const uint32_t MAR_CHANNELS[22] = {
    25070000, 25100000, 25150000, 25200000,     // 25 MHz group
    26010000, 26020000, 26030000, 26040000,     // 26 MHz group A
    26050000, 26060000, 26070000, 26080000,
    26090000, 26100000, 26110000, 26120000,
    26130000, 26140000, 26150000, 26160000,
    26170000, 26175000,                         // distress / calling
};

inline int marFindChannel(uint32_t dialHz, uint32_t toleranceHz = 600) {
    for (int i = 0; i < 22; i++) {
        uint32_t diff = dialHz > MAR_CHANNELS[i]
                        ? dialHz - MAR_CHANNELS[i]
                        : MAR_CHANNELS[i] - dialHz;
        if (diff <= toleranceHz) return i;
    }
    return -1;
}

inline const char* marChannelLabel(int idx, char* buf) {
    if (idx < 0 || idx >= 22) { buf[0]=0; return buf; }
    snprintf(buf, 6, "M%d", idx + 1);
    return buf;
}
