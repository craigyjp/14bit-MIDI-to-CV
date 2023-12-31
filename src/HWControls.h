#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include "TButton.h"

// Notes

#define NOTE1 19
#define NOTE2 18
#define NOTE3 4
#define NOTE4 5
#define NOTE5 6
#define NOTE6 7
#define NOTE7 23
#define NOTE8 22

#define VELOCITY1 15
#define VELOCITY2 14
#define VELOCITY3 13
#define VELOCITY4 29
#define VELOCITY5 8
#define VELOCITY6 9
#define VELOCITY7 10
#define VELOCITY8 11

#define PITCHBEND 12
#define WHEEL 24
#define AFTERTOUCH 25
#define BREATH 28

//Gate outputs
#define GATE_NOTE1 0
#define GATE_NOTE2 1
#define GATE_NOTE3 2
#define GATE_NOTE4 3
#define GATE_NOTE5 4
#define GATE_NOTE6 5
#define GATE_NOTE7 6
#define GATE_NOTE8 7

#define CLOCK_LED 8
#define CLOCK_RESET 9
#define PITCHBEND_LED 10
#define MOD_LED 11
#define AFTERTOUCH_LED 12
#define BREATH_LED 13
#define RESET 14

#define NOTE1_LED 16
#define NOTE2_LED 17
#define NOTE3_LED 18
#define NOTE4_LED 19
#define NOTE5_LED 20
#define NOTE6_LED 21
#define NOTE7_LED 22
#define NOTE8_LED 23

#define VELOCITY1_LED 24
#define VELOCITY2_LED 25
#define VELOCITY3_LED 26
#define VELOCITY4_LED 27
#define VELOCITY5_LED 28
#define VELOCITY6_LED 29
#define VELOCITY7_LED 30
#define VELOCITY8_LED 31


//Encoder or buttons
#define ENC_A 38
#define ENC_B 37
#define RECALL_SW 36

#define ENCODER_PARAMA 41
#define ENCODER_PARAMB 40
#define PARAM_SW 39

#define SAVE_SW 33
#define SETTINGS_SW 34
#define BACK_SW 35

#define DEBOUNCE 30

static long encPrevious = 0;
static long param_encPrevious = 0;

TButton recallButton{ RECALL_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton saveButton{ SAVE_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton settingsButton{ SETTINGS_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton backButton{ BACK_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };

Encoder encoder(ENC_B, ENC_A);  //This often needs the pins swapping depending on the encoder
Encoder param_encoder(ENCODER_PARAMA, ENCODER_PARAMB);

void setupHardware() {

  analogWriteResolution(14);

  pinMode(NOTE1, OUTPUT);
  pinMode(NOTE2, OUTPUT);
  pinMode(NOTE3, OUTPUT);
  pinMode(NOTE4, OUTPUT);
  pinMode(NOTE5, OUTPUT);
  pinMode(NOTE6, OUTPUT);
  pinMode(NOTE7, OUTPUT);
  pinMode(NOTE8, OUTPUT);

  pinMode(VELOCITY1, OUTPUT);
  pinMode(VELOCITY2, OUTPUT);
  pinMode(VELOCITY3, OUTPUT);
  pinMode(VELOCITY4, OUTPUT);
  pinMode(VELOCITY5, OUTPUT);
  pinMode(VELOCITY6, OUTPUT);
  pinMode(VELOCITY7, OUTPUT);
  pinMode(VELOCITY8, OUTPUT);

  pinMode(PITCHBEND, OUTPUT);
  pinMode(WHEEL, OUTPUT);
  pinMode(AFTERTOUCH, OUTPUT);
  pinMode(BREATH, OUTPUT);

  analogWriteFrequency(NOTE1, 9155.27);
  analogWriteFrequency(NOTE2, 9155.27);
  analogWriteFrequency(NOTE3, 9155.27);
  analogWriteFrequency(NOTE4, 9155.27);
  analogWriteFrequency(NOTE5, 9155.27);
  analogWriteFrequency(NOTE6, 9155.27);
  analogWriteFrequency(NOTE7, 9155.27);
  analogWriteFrequency(NOTE8, 9155.27);

  analogWriteFrequency(VELOCITY1, 9155.27);
  analogWriteFrequency(VELOCITY2, 9155.27);
  analogWriteFrequency(VELOCITY3, 9155.27);
  analogWriteFrequency(VELOCITY4, 9155.27);
  analogWriteFrequency(VELOCITY5, 9155.27);
  analogWriteFrequency(VELOCITY6, 9155.27);
  analogWriteFrequency(VELOCITY7, 9155.27);
  analogWriteFrequency(VELOCITY8, 9155.27);

  analogWriteFrequency(PITCHBEND, 9155.27);
  analogWriteFrequency(WHEEL, 9155.27);
  analogWriteFrequency(AFTERTOUCH, 9155.27);
  analogWriteFrequency(BREATH, 9155.27);

  analogWrite(PITCHBEND, 1543);
  analogWrite(WHEEL, 0);
  analogWrite(AFTERTOUCH, 0);
  analogWrite(BREATH, 0);

  analogWrite(NOTE1, 0);
  analogWrite(NOTE2, 0);
  analogWrite(NOTE3, 0);
  analogWrite(NOTE4, 0);
  analogWrite(NOTE5, 0);
  analogWrite(NOTE6, 0);
  analogWrite(NOTE7, 0);
  analogWrite(NOTE8, 0);

  analogWrite(VELOCITY1, 0);
  analogWrite(VELOCITY2, 0);
  analogWrite(VELOCITY3, 0);
  analogWrite(VELOCITY4, 0);
  analogWrite(VELOCITY5, 0);
  analogWrite(VELOCITY6, 0);
  analogWrite(VELOCITY7, 0);
  analogWrite(VELOCITY8, 0);

  pinMode(RECALL_SW, INPUT_PULLUP);
  pinMode(PARAM_SW, INPUT_PULLUP);
  pinMode(SAVE_SW, INPUT_PULLUP);
  pinMode(SETTINGS_SW, INPUT_PULLUP);
  pinMode(BACK_SW, INPUT_PULLUP);

  CC_MAP[0][3] = NOTE1;
  CC_MAP[1][3] = NOTE2;
  CC_MAP[2][3] = NOTE3;
  CC_MAP[3][3] = NOTE4;
  CC_MAP[4][3] = NOTE5;
  CC_MAP[5][3] = NOTE6;
  CC_MAP[6][3] = NOTE7;
  CC_MAP[7][3] = NOTE8;
  CC_MAP[8][3] = VELOCITY1;
  CC_MAP[9][3] = VELOCITY2;
  CC_MAP[10][3] = VELOCITY3;
  CC_MAP[11][3] = VELOCITY4;
  CC_MAP[12][3] = VELOCITY5;
  CC_MAP[13][3] = VELOCITY6;
  CC_MAP[14][3] = VELOCITY7;
  CC_MAP[15][3] = VELOCITY8;

  CC_MAP[0][5] = NOTE1_LED;
  CC_MAP[1][5] = NOTE2_LED;
  CC_MAP[2][5] = NOTE3_LED;
  CC_MAP[3][5] = NOTE4_LED;
  CC_MAP[4][5] = NOTE5_LED;
  CC_MAP[5][5] = NOTE6_LED;
  CC_MAP[6][5] = NOTE7_LED;
  CC_MAP[7][5] = NOTE8_LED;
  CC_MAP[8][5] = VELOCITY1_LED;
  CC_MAP[9][5] = VELOCITY2_LED;
  CC_MAP[10][5] = VELOCITY3_LED;
  CC_MAP[11][5] = VELOCITY4_LED;
  CC_MAP[12][5] = VELOCITY5_LED;
  CC_MAP[13][5] = VELOCITY6_LED;
  CC_MAP[14][5] = VELOCITY7_LED;
  CC_MAP[15][5] = VELOCITY8_LED;
}