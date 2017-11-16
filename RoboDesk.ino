#include "LogicData.h"

#ifdef OAK
// Input
const uint8_t MOD_TX  = P5;

// Output
const uint8_t MOD_HS1 = P11;
const uint8_t MOD_HS2 = P10;
const uint8_t MOD_HS3 = P6;
const uint8_t MOD_HS4 = P4;
//const uint8_t MOD_RX  = P5;

// Output
const uint8_t INTF_TX  = LED_BUILTIN;

// Input
const uint8_t INTF_HS1 = P6;
const uint8_t INTF_HS2 = P7;
const uint8_t INTF_HS3 = P8;
const uint8_t INTF_HS4 = P9;

#else // digistump ATTiny85

// Input
const uint8_t MOD_TX  = 2;
const uint8_t MOD_TX_interrupt  = 0;

// Input/Output
const uint8_t MOD_HS1 = 0;
const uint8_t MOD_HS2 = 3;
const uint8_t MOD_HS3 = 4;
const uint8_t MOD_HS4 = 5;

// Output
const uint8_t INTF_TX  = 1;

#endif

LogicData ld(INTF_TX);

static bool ld_started = false;

inline void passThrough(uint8_t out, uint8_t in) {
  digitalWrite(out, digitalRead(in));
}

// Pass module TX straight through to interface RX
void dataPassThrough() {
  if (!ld.is_active()) passThrough(INTF_TX, MOD_TX);
}


uint32_t test_display_on[] = {
    // display ON; display WORD
    0x40611400,
    0x40601dfe,
    0x40681400,
    0x40600515,
    0x40649dfb,
    
    0x40600495,
    0x40600594,
    0x40600455,
    0x40600554,
    0x406004d4,
    0x406005d5,
    0x40600435,
    0x40600534,
    0x406004b4,
    0x406005b5,
    0x40600474,
    0x40600575,
    0x406004f5,
    0x406005f4,
    0x4060040c,
    0x4060050d,
    0x4060048d,
    0x4060058c,
    0x4060044d,
    0x4060054c,
    0x406004cc,
    0x406005cd,
    0x4060042d,
    0x4060052c,
    0x406004ac,
    0x406005ad,
    0x4060046c,
    0x4060056d,
    0x40601dfe,
    0x406004ed,
    0x40649dfb,
    0x406005ec,
    0x4060041d,
    0x4060051c,
    0x4060049c
};

uint32_t test_display_word[] = {
    // display word?
    0x40601dfe,
    0x40600515,
    0x40649dfb
};

uint32_t test_display_off[] = {
    // display OFF
    0x40601dfe,
    0x40600515,
    0x40649dfb,
    0x406e1400
};


void setup() {
  pinMode(MOD_TX, INPUT);
  digitalWrite(MOD_TX, HIGH); // turn on pullups (weird that this is needed, but it is)

  pinMode(MOD_HS1, INPUT_PULLUP);
  pinMode(MOD_HS2, INPUT_PULLUP);
  pinMode(MOD_HS3, INPUT_PULLUP);
  pinMode(MOD_HS4, INPUT_PULLUP);

  dataPassThrough();
  attachInterrupt(MOD_TX_interrupt, dataPassThrough, CHANGE);

  ld.Begin();
  delay(1000);

  ld_started = true;
  unsigned size = sizeof(test_display_on) / sizeof(test_display_on[0]);
  ld.Send(test_display_on, size);
  ld_started = false;

  // HACK testing
  ld.OpenChannel();
}

uint32_t sample = 0x40600400;

void loop() {
  // Monitor panel buttons for our commands and take over when we see one
  // TODO

  // HACK testing
  ld.Send(sample);
  ld.Send(sample);
  ld.Send(sample);
  ld.Delay(300);
  ++sample;
  sample = sample & ~(0x00001000);
}