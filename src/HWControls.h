#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include "TButton.h"

// Notes

#define NOTE1 2
#define NOTE2 3
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

//Encoder or buttons
#define ENC_A 38
#define ENC_B 37
#define RECALL_SW 36

#define ENCODER_PARAMA 41
#define ENCODER_PARAMB 40
#define ENCODER_PREVIOUS 39

#define SAVE_SW 33
#define SETTINGS_SW 34
#define BACK_SW 35

#define DEBOUNCE 30

static long encPrevious = 0;
static long param_encPrevious = 0;

TButton recallButton{RECALL_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION};
TButton saveButton{SAVE_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION};
TButton settingsButton{SETTINGS_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION};
TButton backButton{BACK_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION};
TButton paramButton{ENCODER_PREVIOUS, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION};
Encoder encoder(ENC_B, ENC_A); //This often needs the pins swapping depending on the encoder
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

  pinMode(RECALL_SW, INPUT_PULLUP);
  pinMode(ENCODER_PREVIOUS, INPUT_PULLUP);
  pinMode(SAVE_SW, INPUT_PULLUP);
  pinMode(SETTINGS_SW, INPUT_PULLUP);
  pinMode(BACK_SW, INPUT_PULLUP);
}