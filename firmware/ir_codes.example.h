#pragma once
#include <Arduino.h>

// Copy this file to ir_codes.h and paste your real codes there.
// DO NOT commit ir_codes.h (it is ignored by .gitignore).

// Carrier frequency (typical: 38 kHz)
static constexpr int IR_KHZ = 38;

// Lengths: set these to match your real arrays
static constexpr int IR_AIRE1_LEN = 0;
static constexpr int IR_AIRE2_LEN = 0;

// --- Air conditioner #1 (example placeholders) ---
static const uint16_t IR_AIRE1_ON[]   = { /* ... */ };
static const uint16_t IR_AIRE1_OFF[]  = { /* ... */ };
static const uint16_t IR_AIRE1_UP[]   = { /* ... */ };
static const uint16_t IR_AIRE1_DOWN[] = { /* ... */ };

// --- Air conditioner #2 (example placeholders) ---
static const uint16_t IR_AIRE2_ON[]   = { /* ... */ };
static const uint16_t IR_AIRE2_OFF[]  = { /* ... */ };
static const uint16_t IR_AIRE2_UP[]   = { /* ... */ };
static const uint16_t IR_AIRE2_DOWN[] = { /* ... */ };
