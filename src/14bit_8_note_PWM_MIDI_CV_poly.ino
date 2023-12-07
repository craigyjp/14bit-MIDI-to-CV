/*
      MIDI2CV_Poly
      Copyright (C) 2020 Craig Barnes

      A big thankyou to Elkayem for his midi to cv code
      A big thankyou to ElectroTechnique for his polyphonic tsynth that I used for the poly notes routine

      This program is free software: you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation, either version 3 of the License, or
      (at your option) any later version.

      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License <http://www.gnu.org/licenses/> for more details.
*/

#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <MIDI.h>
#include "MidiCC.h"
#include "Constants.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include <USBHost_t36.h>
#include "HWControls.h"
#include "EepromMgr.h"
#include "Settings.h"
#include <ShiftRegister74HC595.h>
#include <RoxMux.h>

// OLED I2C is used on pins 18 and 19 for Teensy 3.x

// Voices available
#define NO_OF_VOICES 8

// 10 octave keyboard on a 3.3v powered PWM output scaled to 10V

#define NOTE_SF 129.5f

#define PARAMETER 0      //The main page for displaying the current patch and control (parameter) changes
#define RECALL 1         //Patches list
#define SAVE 2           //Save patch page
#define PATCH 3          // Show current patch bypassing PARAMETER
#define PATCHNAMING 4    // Patch naming page
#define DELETE 5         //Delete patch page
#define DELETEMSG 6      //Delete patch message page
#define SETTINGS 7       //Settings page
#define SETTINGSVALUE 8  //Settings page
#define CCPARAMS 9       //Params Page

unsigned int state = PARAMETER;

#include "SSD1306Display.h"

boolean cardStatus = false;

struct VoiceAndNote {
  int note;
  int velocity;
  long timeOn;
};

struct VoiceAndNote voices[NO_OF_VOICES] = {
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 }
};

boolean voiceOn[NO_OF_VOICES] = { false, false, false, false, false, false, false, false };
int voiceToReturn = -1;        //Initialise to 'null'
long earliestTime = millis();  //For voice allocation - initialise to now
int prevNote = 0;              //Initialised to middle value
bool notes[128] = { 0 }, initial_loop = 1;
int8_t noteOrder[40] = { 0 }, orderIndx = { 0 };

// MIDI setup

//USB HOST MIDI Class Compliant
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
int channel = 1;

int patchNo = 1;  //Current patch no

// parameters: <number of shift registers> (data pin, clock pin, latch pin)
ShiftRegister74HC595<4> sr(30, 31, 32);

RoxButton paramButton;

void setup() {

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  setupDisplay();
  setUpSettings();
  setupHardware();

  paramButton.begin();
  paramButton.setDoublePressThreshold(300);

  cardStatus = SD.begin(BUILTIN_SDCARD);
  if (cardStatus) {
    Serial.println("SD card is connected");
    //Get patch numbers and names from SD card
    loadPatches();
    if (patches.size() == 0) {
      //save an initialised patch to SD card
      savePatch("1", INITPATCH);
      loadPatches();
    }
  } else {
    Serial.println("SD card is not connected or unusable");
    showPatchPage("No SD", "conn'd / usable");
  }

  //USB HOST MIDI Class Compliant
  delay(300);  //Wait to turn on USB Host
  myusb.begin();
  midi1.setHandleControlChange(myControlChange);
  midi1.setHandleNoteOff(myNoteOff);
  midi1.setHandleNoteOn(myNoteOn);
  midi1.setHandlePitchChange(myPitchBend);
  Serial.println("USB HOST MIDI Class Compliant Listening");

  //MIDI 5 Pin DIN
  MIDI.begin(0);
  MIDI.setHandleNoteOn(myNoteOn);
  MIDI.setHandleNoteOff(myNoteOff);
  MIDI.setHandlePitchBend(myPitchBend);
  MIDI.setHandleControlChange(myControlChange);
  MIDI.setHandleAfterTouchChannel(myAfterTouch);
  MIDI.setHandleClock(myClock);
  MIDI.setHandleStart(myStart);
  MIDI.setHandleStop(myStop);
  MIDI.setHandleContinue(myContinue);
  Serial.println("MIDI In DIN Listening");

  //USB Client MIDI
  usbMIDI.setHandleControlChange(myControlChange);
  usbMIDI.setHandleNoteOff(myNoteOff);
  usbMIDI.setHandleNoteOn(myNoteOn);
  usbMIDI.setHandlePitchChange(myPitchBend);
  usbMIDI.setHandleAfterTouchChannel(myAfterTouch);
  usbMIDI.setHandleClock(myClock);
  usbMIDI.setHandleStart(myStart);
  usbMIDI.setHandleStop(myStop);
  usbMIDI.setHandleContinue(myContinue);
  Serial.println("USB Client MIDI Listening");

  // Read Settings from EEPROM
  for (int i = 0; i < 8; i++) {
    EEPROM.get(ADDR_SF_ADJUST + i * sizeof(float), sfAdj[i]);
    if ((sfAdj[i] < 0.9f) || (sfAdj[i] > 1.1f) || isnan(sfAdj[i])) sfAdj[i] = 1.0f;
  }

  // Keyboard mode
  keyboardMode = EEPROM.read(ADDR_KEYBOARD_MODE);

  // MIDI Channel
  midiChannel = EEPROM.read(EEPROM_MIDI_CH);
  gateChannel = EEPROM.read(EEPROM_GATE_CH);

  // Transpose amount
  transpose = EEPROM.read(ADDR_TRANSPOSE);
  oldeepromtranspose = EEPROM.read(ADDR_TRANSPOSE);
  transpose = transpose - 12;

  // Octave Range
  octave = EEPROM.read(ADDR_OCTAVE);
  oldeepromOctave = EEPROM.read(ADDR_OCTAVE);

  // Set defaults if EEPROM not initialized
  if (octave == 0) realoctave = -24;
  if (octave == 1) realoctave = -12;
  if (octave == 2) realoctave = 0;
  if (octave == 3) realoctave = 12;
  if (octave == 4) realoctave = 24;

  sr.set(CLOCK_RESET, LOW);

  recallPatch(1);
}

void myClock() {
  if (millis() > clock_timeout + 300) clock_count = 0;  // Prevents Clock from starting in between quarter notes after clock is restarted!
  clock_timeout = millis();

  if (clock_count == 0) {
    //digitalWriteFast(CLOCK_LED, HIGH);  // Start clock pulse
    sr.set(CLOCK_LED, HIGH);
    clock_timer = millis();
  }
  clock_count++;
  if (clock_count == 24) {  // MIDI timing clock sends 24 pulses per quarter note.  Sent pulse only once every 24 pulses
    clock_count = 0;
  }
}

// void myClock() {
//   if (Clock <= 4) {
//     if (SetTempoActive) {
//       digitalWrite(CLOCK_LED, HIGH);
//     }
//   } else {
//     digitalWrite(CLOCK_LED, LOW);
//   }
//   Clock = (Clock + 1) % 24;
// }

void myStart() {
  Clock = 0;
  sr.set(RESET, HIGH);
  reset_timer = millis();
}

void myStop() {
  sr.set(CLOCK_RESET, HIGH);
  sr.set(CLOCK_LED, LOW);
  sr.set(CLOCK_RESET, LOW);
}

void myContinue() {
  Clock = 0;
}

void myPitchBend(byte channel, int bend) {
  if (channel == midiChannel) {
    int newbend = map(bend, -8191, 8192, 0, 3087);
    analogWrite(PITCHBEND, newbend);
    sr.set(PITCHBEND_LED, HIGH);
    pitchbend_timer = millis();
  }
}

void myControlChange(byte channel, byte number, byte value) {
  if (channel == midiChannel) {
    if (number == 1) {
      int newvalue = value;
      newvalue = map(newvalue, 0, 127, 0, 7720);
      analogWrite(WHEEL, newvalue);
      sr.set(MOD_LED, HIGH);
      mod_timer = millis();
    }

    if (number == 2) {
      int newvalue = value;
      newvalue = map(newvalue, 0, 127, 0, 7720);
      analogWrite(BREATH, newvalue);
      sr.set(BREATH_LED, HIGH);
      breath_timer = millis();
    }
  }

  for (int i = 0; i < 16; i++) {
    if ((CC_MAP[i][2] == 0 && CC_MAP[i][4] == 2) || (CC_MAP[i][2] == 0 && CC_MAP[i][4] == 3)) {
      if (CC_MAP[i][1] == channel && CC_MAP[i][0] == number) {
        int newvalue = value;

        if (CC_MAP[i][4] == 2) {
          newvalue = map(newvalue, 0, 127, 0, 7720);
          analogWrite(CC_MAP[i][3], newvalue);
          sr.set(CC_MAP[i][5], HIGH);
          outputLEDS[i] = millis();
        }

        if (CC_MAP[i][4] == 3) {
          newvalue = map(newvalue, 0, 127, 0, 15440);
          analogWrite(CC_MAP[i][3], newvalue);
          sr.set(CC_MAP[i][5], HIGH);
          outputLEDS[i] = millis();
        }
      }
    }
  }

  for (int i = 0; i < 16; i++) {
    if ((CC_MAP[i][2] == 0 && CC_MAP[i][4] == 4) || (CC_MAP[i][2] == 0 && CC_MAP[i][4] == 5)) {
      if (CC_MAP[i][1] == channel && 99 == number) {
        value99 = value;
      }
      if (CC_MAP[i][1] == channel && 98 == number) {
        value98 = value;
      }
      if (CC_MAP[i][1] == channel && 6 == number) {
        value6 = value;
      }
      if (CC_MAP[i][1] == channel && 38 == number) {
        value38 = value;
      }

      if (CC_MAP[i][4] == 4) {
        uint16_t combinedNumber = (value6 << 7) | value38;
        combinedNumber = map(combinedNumber, 0, 1023, 0, 7720);
        analogWrite(CC_MAP[i][3], combinedNumber);
        sr.set(CC_MAP[i][5], HIGH);
        outputLEDS[i] = millis();
      }

      if (CC_MAP[i][4] == 5) {
        uint16_t combinedNumber = (value6 << 7) | value38;
        combinedNumber = map(combinedNumber, 0, 1023, 0, 15440);
        analogWrite(CC_MAP[i][3], combinedNumber);
        sr.set(CC_MAP[i][5], HIGH);
        outputLEDS[i] = millis();
      }
    }
  }
}

void myAfterTouch(byte channel, byte value) {
  if (channel == midiChannel) {
    int newvalue = value;
    newvalue = map(newvalue, 0, 127, 0, 7720);
    analogWrite(AFTERTOUCH, newvalue);
    sr.set(AFTERTOUCH_LED, HIGH);
    aftertouch_timer = millis();
  }
}

void updatepolyCount() {
  showCurrentParameterPage("Poly Count", String(polycount) + " Notes");
  freeGates = (8 - polycount);
  GATE_NOTES[0] = gate1;
  GATE_NOTES[1] = gate2;
  GATE_NOTES[2] = gate3;
  GATE_NOTES[3] = gate4;
  GATE_NOTES[4] = gate5;
  GATE_NOTES[5] = gate6;
  GATE_NOTES[6] = gate7;
  GATE_NOTES[7] = gate8;

  updateCCMAP();
}

void updatechannel1() {
  switch (channel1) {
    case 0:
      showCurrentParameterPage("Channel 1", "CC Number " + String(channel1_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 1", "MIDI Chan " + String(channel1_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 1", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 1", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 1", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 1", "NPRN 0-10V");
      break;
  }
}

void updatechannel2() {
  switch (channel2_CC) {
    case 0:
      showCurrentParameterPage("Channel 2", "CC Number " + String(channel2_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 2", "MIDI Chan " + String(channel2_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 2", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 2", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 2", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 2", "NPRN 0-10V");
      break;
  }
}

void updatechannel3() {
  switch (channel3) {
    case 0:
      showCurrentParameterPage("Channel 3", "CC Number " + String(channel3_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 3", "MIDI Chan " + String(channel3_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 3", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 3", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 3", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 3", "NPRN 0-10V");
      break;
  }
}

void updatechannel4() {
  switch (channel4) {
    case 0:
      showCurrentParameterPage("Channel 4", "CC Number " + String(channel4_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 4", "MIDI Chan " + String(channel4_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 4", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 4", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 4", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 4", "NPRN 0-10V");
      break;
  }
}

void updatechannel5() {
  switch (channel5) {
    case 0:
      showCurrentParameterPage("Channel 5", "CC Number " + String(channel5_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 5", "MIDI Chan " + String(channel5_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 5", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 5", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 5", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 5", "NPRN 0-10V");
      break;
  }
}

void updatechannel6() {
  switch (channel6) {
    case 0:
      showCurrentParameterPage("Channel 6", "CC Number " + String(channel6_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 6", "MIDI Chan " + String(channel6_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 6", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 6", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 6", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 6", "NPRN 0-10V");
      break;
  }
}

void updatechannel7() {
  switch (channel7) {
    case 0:
      showCurrentParameterPage("Channel 7", "CC Number " + String(channel7_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 7", "MIDI Chan " + String(channel7_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 7", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 7", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 7", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 7", "NPRN 0-10V");
      break;
  }
}

void updatechannel8() {
  switch (channel8) {
    case 0:
      showCurrentParameterPage("Channel 8", "CC Number " + String(channel8_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 8", "MIDI Chan " + String(channel8_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 8", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 8", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 8", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 8", "NPRN 0-10V");
      break;
  }
}

void updatechannel9() {
  switch (channel9) {
    case 0:
      showCurrentParameterPage("Channel 9", "CC Number " + String(channel9_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 9", "MIDI Chan " + String(channel9_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 9", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 9", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 9", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 9", "NPRN 0-10V");
      break;
  }
}

void updatechannel10() {
  switch (channel10) {
    case 0:
      showCurrentParameterPage("Channel 10", "CC Number " + String(channel10_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 10", "MIDI Chan " + String(channel10_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 10", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 10", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 10", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 10", "NPRN 0-10V");
      break;
  }
}

void updatechannel11() {
  switch (channel11) {
    case 0:
      showCurrentParameterPage("Channel 11", "CC Number " + String(channel11_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 11", "MIDI Chan " + String(channel11_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 11", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 11", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 11", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 11", "NPRN 0-10V");
      break;
  }
}

void updatechannel12() {
  switch (channel12) {
    case 0:
      showCurrentParameterPage("Channel 12", "CC Number " + String(channel12_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 12", "MIDI Chan " + String(channel12_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 12", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 12", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 12", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 12", "NPRN 0-10V");
      break;
  }
}

void updatechannel13() {
  switch (channel13) {
    case 0:
      showCurrentParameterPage("Channel 13", "CC Number " + String(channel13_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 13", "MIDI Chan " + String(channel13_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 13", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 13", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 13", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 13", "NPRN 0-10V");
      break;
  }
}

void updatechannel14() {
  switch (channel14) {
    case 0:
      showCurrentParameterPage("Channel 14", "CC Number " + String(channel14_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 14", "MIDI Chan " + String(channel14_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 14", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 14", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 14", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 14", "NPRN 0-10V");
      break;
  }
}

void updatechannel15() {
  switch (channel15) {
    case 0:
      showCurrentParameterPage("Channel 15", "CC Number " + String(channel15_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 15", "MIDI Chan " + String(channel15_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 15", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 15", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 15", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 15", "NPRN 0-10V");
      break;
  }
}

void updatechannel16() {
  switch (channel16) {
    case 0:
      showCurrentParameterPage("Channel 16", "CC Number " + String(channel16_CC));
      break;

    case 1:
      showCurrentParameterPage("Channel 16", "MIDI Chan " + String(channel16_MIDI));
      break;

    case 2:
      showCurrentParameterPage("Channel 16", "CC 0-5V");
      break;

    case 3:
      showCurrentParameterPage("Channel 16", "CC 0-10V");
      break;

    case 4:
      showCurrentParameterPage("Channel 16", "NPRN 0-5V");
      break;

    case 5:
      showCurrentParameterPage("Channel 16", "NPRN 0-10V");
      break;
  }
}

void updategate1() {
  showCurrentParameterPage("Gate 1", "Note " + String(gate1));
}

void updategate2() {
  showCurrentParameterPage("Gate 2", "Note " + String(gate2));
}

void updategate3() {
  showCurrentParameterPage("Gate 3", "Note " + String(gate3));
}

void updategate4() {
  showCurrentParameterPage("Gate 4", "Note " + String(gate4));
}

void updategate5() {
  showCurrentParameterPage("Gate 5", "Note " + String(gate5));
}

void updategate6() {
  showCurrentParameterPage("Gate 6", "Note " + String(gate6));
}

void updategate7() {
  showCurrentParameterPage("Gate 7", "Note " + String(gate7));
}

void updategate8() {
  showCurrentParameterPage("Gate 8", "Note " + String(gate8));
}

void commandTopNote() {
  int topNote = 0;
  bool noteActive = false;

  for (int i = 0; i < 128; i++) {
    if (notes[i]) {
      topNote = i;
      noteActive = true;
    }
  }

  if (noteActive)
    commandNote(topNote);
  else  // All notes are off, turn off gate
    sr.set(GATE_NOTE1, LOW);
  sr.set(NOTE1_LED, LOW);
}

void commandBottomNote() {
  int bottomNote = 0;
  bool noteActive = false;

  for (int i = 127; i >= 0; i--) {
    if (notes[i]) {
      bottomNote = i;
      noteActive = true;
    }
  }

  if (noteActive)
    commandNote(bottomNote);
  else  // All notes are off, turn off gate
    sr.set(GATE_NOTE1, LOW);
  sr.set(NOTE1_LED, LOW);
}

void commandLastNote() {

  int8_t noteIndx;

  for (int i = 0; i < 40; i++) {
    noteIndx = noteOrder[mod(orderIndx - i, 40)];
    if (notes[noteIndx]) {
      commandNote(noteIndx);
      return;
    }
  }
  sr.set(GATE_NOTE1, LOW);  // All notes are off
  sr.set(NOTE1_LED, LOW);
}

void commandNote(int noteMsg) {
  unsigned int mV = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
  analogWrite(NOTE1, mV);
  sr.set(GATE_NOTE1, HIGH);
  sr.set(NOTE1_LED, HIGH);
}

void commandTopNoteUni() {
  int topNote = 0;
  bool noteActive = false;

  for (int i = 0; i < 128; i++) {
    if (notes[i]) {
      topNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNoteUni(topNote);
  } else {  // All notes are off, turn off gate
    updateGates(0);
  }
}

void commandBottomNoteUni() {
  int bottomNote = 0;
  bool noteActive = false;

  for (int i = 127; i >= 0; i--) {
    if (notes[i]) {
      bottomNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNoteUni(bottomNote);
  } else {  // All notes are off, turn off gate
    updateGates(0);
  }
}

void commandLastNoteUni() {

  int8_t noteIndx;

  for (int i = 0; i < 40; i++) {
    noteIndx = noteOrder[mod(orderIndx - i, 40)];
    if (notes[noteIndx]) {
      commandNoteUni(noteIndx);
      return;
    }
  }
  updateGates(0);
  // All notes are off
}

void updateGates(int gatestate) {
  switch (polycount) {
    case 1:
      sr.set(GATE_NOTE1, gatestate);
      sr.set(NOTE1_LED, gatestate);
      break;

    case 2:
      sr.set(GATE_NOTE1, gatestate);
      sr.set(GATE_NOTE2, gatestate);
      sr.set(NOTE1_LED, gatestate);
      sr.set(NOTE2_LED, gatestate);
      break;

    case 3:
      sr.set(GATE_NOTE1, gatestate);
      sr.set(GATE_NOTE2, gatestate);
      sr.set(GATE_NOTE3, gatestate);
      sr.set(NOTE1_LED, gatestate);
      sr.set(NOTE2_LED, gatestate);
      sr.set(NOTE3_LED, gatestate);
      break;

    case 4:
      sr.set(GATE_NOTE1, gatestate);
      sr.set(GATE_NOTE2, gatestate);
      sr.set(GATE_NOTE3, gatestate);
      sr.set(GATE_NOTE4, gatestate);
      sr.set(NOTE1_LED, gatestate);
      sr.set(NOTE2_LED, gatestate);
      sr.set(NOTE3_LED, gatestate);
      sr.set(NOTE4_LED, gatestate);
      break;

    case 5:
      sr.set(GATE_NOTE1, gatestate);
      sr.set(GATE_NOTE2, gatestate);
      sr.set(GATE_NOTE3, gatestate);
      sr.set(GATE_NOTE4, gatestate);
      sr.set(GATE_NOTE5, gatestate);
      sr.set(NOTE1_LED, gatestate);
      sr.set(NOTE2_LED, gatestate);
      sr.set(NOTE3_LED, gatestate);
      sr.set(NOTE4_LED, gatestate);
      sr.set(NOTE5_LED, gatestate);
      break;

    case 6:
      sr.set(GATE_NOTE1, gatestate);
      sr.set(GATE_NOTE2, gatestate);
      sr.set(GATE_NOTE3, gatestate);
      sr.set(GATE_NOTE4, gatestate);
      sr.set(GATE_NOTE5, gatestate);
      sr.set(GATE_NOTE6, gatestate);
      sr.set(NOTE1_LED, gatestate);
      sr.set(NOTE2_LED, gatestate);
      sr.set(NOTE3_LED, gatestate);
      sr.set(NOTE4_LED, gatestate);
      sr.set(NOTE5_LED, gatestate);
      sr.set(NOTE6_LED, gatestate);
      break;

    case 7:
      sr.set(GATE_NOTE1, gatestate);
      sr.set(GATE_NOTE2, gatestate);
      sr.set(GATE_NOTE3, gatestate);
      sr.set(GATE_NOTE4, gatestate);
      sr.set(GATE_NOTE5, gatestate);
      sr.set(GATE_NOTE6, gatestate);
      sr.set(GATE_NOTE7, gatestate);
      sr.set(NOTE1_LED, gatestate);
      sr.set(NOTE2_LED, gatestate);
      sr.set(NOTE3_LED, gatestate);
      sr.set(NOTE4_LED, gatestate);
      sr.set(NOTE5_LED, gatestate);
      sr.set(NOTE6_LED, gatestate);
      sr.set(NOTE7_LED, gatestate);
      break;

    case 8:
      sr.set(GATE_NOTE1, gatestate);
      sr.set(GATE_NOTE2, gatestate);
      sr.set(GATE_NOTE3, gatestate);
      sr.set(GATE_NOTE4, gatestate);
      sr.set(GATE_NOTE5, gatestate);
      sr.set(GATE_NOTE6, gatestate);
      sr.set(GATE_NOTE7, gatestate);
      sr.set(GATE_NOTE8, gatestate);
      sr.set(NOTE1_LED, gatestate);
      sr.set(NOTE2_LED, gatestate);
      sr.set(NOTE3_LED, gatestate);
      sr.set(NOTE4_LED, gatestate);
      sr.set(NOTE5_LED, gatestate);
      sr.set(NOTE6_LED, gatestate);
      sr.set(NOTE7_LED, gatestate);
      sr.set(NOTE8_LED, gatestate);
  }
}

void commandNoteUni(int noteMsg) {
  switch (polycount) {
    case 1:
      mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
      analogWrite(NOTE1, mV1);
      updateGates(1);
      break;

    case 2:
      mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
      analogWrite(NOTE1, mV1);
      mV2 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
      analogWrite(NOTE2, mV2);
      updateGates(1);
      break;

    case 3:
      mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
      analogWrite(NOTE1, mV1);
      mV2 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
      analogWrite(NOTE2, mV2);
      mV3 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
      analogWrite(NOTE3, mV3);
      updateGates(1);
      break;

    case 4:
      mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
      analogWrite(NOTE1, mV1);
      mV2 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
      analogWrite(NOTE2, mV2);
      mV3 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
      analogWrite(NOTE3, mV3);
      mV4 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
      analogWrite(NOTE4, mV4);
      updateGates(1);
      break;

    case 5:
      mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
      analogWrite(NOTE1, mV1);
      mV2 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
      analogWrite(NOTE2, mV2);
      mV3 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
      analogWrite(NOTE3, mV3);
      mV4 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
      analogWrite(NOTE4, mV4);
      mV5 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
      analogWrite(NOTE5, mV5);
      updateGates(1);
      break;

    case 6:
      mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
      analogWrite(NOTE1, mV1);
      mV2 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
      analogWrite(NOTE2, mV2);
      mV3 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
      analogWrite(NOTE3, mV3);
      mV4 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
      analogWrite(NOTE4, mV4);
      mV5 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
      analogWrite(NOTE5, mV5);
      mV6 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[5] + 0.5);
      analogWrite(NOTE6, mV6);
      updateGates(1);
      break;

    case 7:
      mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
      analogWrite(NOTE1, mV1);
      mV2 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
      analogWrite(NOTE2, mV2);
      mV3 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
      analogWrite(NOTE3, mV3);
      mV4 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
      analogWrite(NOTE4, mV4);
      mV5 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
      analogWrite(NOTE5, mV5);
      mV6 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[5] + 0.5);
      analogWrite(NOTE6, mV6);
      mV7 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[6] + 0.5);
      analogWrite(NOTE7, mV7);
      updateGates(1);
      break;

    case 8:
      mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
      analogWrite(NOTE1, mV1);
      mV2 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
      analogWrite(NOTE2, mV2);
      mV3 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
      analogWrite(NOTE3, mV3);
      mV4 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
      analogWrite(NOTE4, mV4);
      mV5 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
      analogWrite(NOTE5, mV5);
      mV6 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[5] + 0.5);
      analogWrite(NOTE6, mV6);
      mV7 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[6] + 0.5);
      analogWrite(NOTE7, mV7);
      mV8 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[7] + 0.5);
      analogWrite(NOTE8, mV8);
      updateGates(1);
      break;
  }
}

void myNoteOn(byte channel, byte note, byte velocity) {
  if (channel == midiChannel) {
    //Check for out of range notes
    if (note < 0 || note > 127) return;

    prevNote = note;
    if (keyboardMode == 0) {
      switch (getVoiceNo(-1)) {
        case 1:
          voices[0].note = note;
          voices[0].velocity = velocity;
          voices[0].timeOn = millis();
          updateVoice1();
          sr.set(GATE_NOTE1, HIGH);
          sr.set(NOTE1_LED, HIGH);
          voiceOn[0] = true;
          break;

        case 2:
          voices[1].note = note;
          voices[1].velocity = velocity;
          voices[1].timeOn = millis();
          updateVoice2();
          sr.set(GATE_NOTE2, HIGH);
          sr.set(NOTE2_LED, HIGH);
          voiceOn[1] = true;
          break;

        case 3:
          voices[2].note = note;
          voices[2].velocity = velocity;
          voices[2].timeOn = millis();
          updateVoice3();
          sr.set(GATE_NOTE3, HIGH);
          sr.set(NOTE3_LED, HIGH);
          voiceOn[2] = true;
          break;

        case 4:
          voices[3].note = note;
          voices[3].velocity = velocity;
          voices[3].timeOn = millis();
          updateVoice4();
          sr.set(GATE_NOTE4, HIGH);
          sr.set(NOTE4_LED, HIGH);
          voiceOn[3] = true;
          break;

        case 5:
          voices[4].note = note;
          voices[4].velocity = velocity;
          voices[4].timeOn = millis();
          updateVoice5();
          sr.set(GATE_NOTE5, HIGH);
          sr.set(NOTE5_LED, HIGH);
          voiceOn[4] = true;
          break;

        case 6:
          voices[5].note = note;
          voices[5].velocity = velocity;
          voices[5].timeOn = millis();
          updateVoice6();
          sr.set(GATE_NOTE6, HIGH);
          sr.set(NOTE6_LED, HIGH);
          voiceOn[5] = true;
          break;

        case 7:
          voices[6].note = note;
          voices[6].velocity = velocity;
          voices[6].timeOn = millis();
          updateVoice7();
          sr.set(GATE_NOTE7, HIGH);
          sr.set(NOTE7_LED, HIGH);
          voiceOn[6] = true;
          break;

        case 8:
          voices[7].note = note;
          voices[7].velocity = velocity;
          voices[7].timeOn = millis();
          updateVoice8();
          sr.set(GATE_NOTE8, HIGH);
          sr.set(NOTE8_LED, HIGH);
          voiceOn[7] = true;
          break;
      }
    } else if (keyboardMode == 4 || keyboardMode == 5 || keyboardMode == 6) {
      noteMsg = note;

      if (velocity == 0) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }

      unsigned int velmV = map(velocity, 0, 127, 0, 8191);
      analogWrite(VELOCITY1, velmV);
      switch (keyboardMode) {
        case 4:
          commandTopNote();
          break;
        case 5:
          commandBottomNote();
          break;
        case 6:
          if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
            orderIndx = (orderIndx + 1) % 40;
            noteOrder[orderIndx] = noteMsg;
          }
          commandLastNote();
          break;
      }
    } else if (keyboardMode == 1 || keyboardMode == 2 || keyboardMode == 3) {
      noteMsg = note;

      if (velocity == 0) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }

      unsigned int velmV = map(velocity, 0, 127, 0, 8191);
      switch (polycount) {
        case 1:
          analogWrite(VELOCITY1, velmV);
          break;

        case 2:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          break;

        case 3:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          break;

        case 4:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          break;

        case 5:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          analogWrite(VELOCITY5, velmV);
          break;

        case 6:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          analogWrite(VELOCITY5, velmV);
          analogWrite(VELOCITY6, velmV);
          break;

        case 7:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          analogWrite(VELOCITY5, velmV);
          analogWrite(VELOCITY6, velmV);
          analogWrite(VELOCITY7, velmV);
          break;

        case 8:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          analogWrite(VELOCITY5, velmV);
          analogWrite(VELOCITY6, velmV);
          analogWrite(VELOCITY7, velmV);
          analogWrite(VELOCITY8, velmV);
          break;
      }
      switch (keyboardMode) {
        case 1:
          commandTopNoteUni();
          break;
        case 2:
          commandBottomNoteUni();
          break;
        case 3:
          if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
            orderIndx = (orderIndx + 1) % 40;
            noteOrder[orderIndx] = noteMsg;
          }
          commandLastNoteUni();
          break;
      }
    }
  }
  if (channel == gateChannel) {
    for (uint8_t pin_index = polycount; pin_index < 8; pin_index++) {
      if (GATE_NOTES[pin_index] == note) {
        sr.set(pin_index, HIGH);
        sr.set((pin_index + 16), HIGH);
      }
    }
  }
}

void myNoteOff(byte channel, byte note, byte velocity) {
  if (channel == midiChannel) {
    if (keyboardMode == 0) {
      switch (getVoiceNo(note)) {
        case 1:
          sr.set(GATE_NOTE1, LOW);
          sr.set(NOTE1_LED, LOW);
          voices[0].note = -1;
          voiceOn[0] = false;
          break;
        case 2:
          sr.set(GATE_NOTE2, LOW);
          sr.set(NOTE2_LED, LOW);
          voices[1].note = -1;
          voiceOn[1] = false;
          break;
        case 3:
          sr.set(GATE_NOTE3, LOW);
          sr.set(NOTE3_LED, LOW);
          voices[2].note = -1;
          voiceOn[2] = false;
          break;
        case 4:
          sr.set(GATE_NOTE4, LOW);
          sr.set(NOTE4_LED, LOW);
          voices[3].note = -1;
          voiceOn[3] = false;
          break;
        case 5:
          sr.set(GATE_NOTE5, LOW);
          sr.set(NOTE5_LED, LOW);
          voices[4].note = -1;
          voiceOn[4] = false;
          break;
        case 6:
          sr.set(GATE_NOTE6, LOW);
          sr.set(NOTE6_LED, LOW);
          voices[5].note = -1;
          voiceOn[5] = false;
          break;
        case 7:
          sr.set(GATE_NOTE7, LOW);
          sr.set(NOTE7_LED, LOW);
          voices[6].note = -1;
          voiceOn[6] = false;
          break;
        case 8:
          sr.set(GATE_NOTE8, LOW);
          sr.set(NOTE8_LED, LOW);
          voices[7].note = -1;
          voiceOn[7] = false;
          break;
      }
    } else if (keyboardMode == 4 || keyboardMode == 5 || keyboardMode == 6) {

      noteMsg = note;

      if (velocity == 0 || velocity == 64) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }

      // Pins NP_SEL1 and NP_SEL2 indictate note priority

      unsigned int velmV = map(velocity, 0, 127, 0, 8191);
      analogWrite(VELOCITY1, velmV);
      switch (keyboardMode) {
        case 4:
          commandTopNote();
          break;
        case 5:
          commandBottomNote();
          break;
        case 6:
          if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
            orderIndx = (orderIndx + 1) % 40;
            noteOrder[orderIndx] = noteMsg;
          }
          commandLastNote();
          break;
      }
    } else if (keyboardMode == 1 || keyboardMode == 2 || keyboardMode == 3) {
      noteMsg = note;

      if (velocity == 0 || velocity == 64) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }

      unsigned int velmV = map(velocity, 0, 127, 0, 8191);
      switch (polycount) {
        case 1:
          analogWrite(VELOCITY1, velmV);
          break;

        case 2:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          break;

        case 3:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          break;

        case 4:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          break;

        case 5:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          analogWrite(VELOCITY5, velmV);
          break;

        case 6:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          analogWrite(VELOCITY5, velmV);
          analogWrite(VELOCITY6, velmV);
          break;

        case 7:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          analogWrite(VELOCITY5, velmV);
          analogWrite(VELOCITY6, velmV);
          analogWrite(VELOCITY7, velmV);
          break;

        case 8:
          analogWrite(VELOCITY1, velmV);
          analogWrite(VELOCITY2, velmV);
          analogWrite(VELOCITY3, velmV);
          analogWrite(VELOCITY4, velmV);
          analogWrite(VELOCITY5, velmV);
          analogWrite(VELOCITY6, velmV);
          analogWrite(VELOCITY7, velmV);
          analogWrite(VELOCITY8, velmV);
          break;
      }
      switch (keyboardMode) {
        case 1:
          commandTopNoteUni();
          break;
        case 2:
          commandBottomNoteUni();
          break;
        case 3:
          if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
            orderIndx = (orderIndx + 1) % 40;
            noteOrder[orderIndx] = noteMsg;
          }
          commandLastNoteUni();
          break;
      }
    }
  }
  if (channel == gateChannel) {
    for (uint8_t pin_index = polycount; pin_index < 8; pin_index++) {
      if (GATE_NOTES[pin_index] == note) {
        sr.set(pin_index, LOW);
        sr.set((pin_index + 16), LOW);
      }
    }
  }
}

int getVoiceNo(int note) {
  voiceToReturn = -1;       //Initialise to 'null'
  earliestTime = millis();  //Initialise to now
  if (note == -1) {
    //NoteOn() - Get the oldest free voice (recent voices may be still on release stage)
    for (int i = 0; i < polycount; i++) {
      if (voices[i].note == -1) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    if (voiceToReturn == -1) {
      //No free voices, need to steal oldest sounding voice
      earliestTime = millis();  //Reinitialise
      for (int i = 0; i < polycount; i++) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    return voiceToReturn + 1;
  } else {
    //NoteOff() - Get voice number from note
    for (int i = 0; i < polycount; i++) {
      if (voices[i].note == note) {
        return i + 1;
      }
    }
  }
  //Shouldn't get here, return voice 1
  return 1;
}

void updateVoice1() {
  unsigned int mV = (unsigned int)((float)(voices[0].note + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
  analogWrite(NOTE1, mV);
  unsigned int velmV = map(voices[0].velocity, 0, 127, 0, 8191);
  analogWrite(VELOCITY1, velmV);
}

void updateVoice2() {
  unsigned int mV = (unsigned int)((float)(voices[1].note + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
  analogWrite(NOTE2, mV);
  unsigned int velmV = map(voices[1].velocity, 0, 127, 0, 8191);
  analogWrite(VELOCITY2, velmV);
}

void updateVoice3() {
  unsigned int mV = (unsigned int)((float)(voices[2].note + transpose + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
  analogWrite(NOTE3, mV);
  unsigned int velmV = map(voices[2].velocity, 0, 127, 0, 8191);
  analogWrite(VELOCITY3, velmV);
}

void updateVoice4() {
  unsigned int mV = (unsigned int)((float)(voices[3].note + transpose + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
  analogWrite(NOTE4, mV);
  unsigned int velmV = map(voices[3].velocity, 0, 127, 0, 8191);
  analogWrite(VELOCITY4, velmV);
}

void updateVoice5() {
  unsigned int mV = (unsigned int)((float)(voices[4].note + transpose + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
  analogWrite(NOTE5, mV);
  unsigned int velmV = map(voices[4].velocity, 0, 127, 0, 8191);
  analogWrite(VELOCITY5, velmV);
}

void updateVoice6() {
  unsigned int mV = (unsigned int)((float)(voices[5].note + transpose + realoctave) * NOTE_SF * sfAdj[5] + 0.5);
  analogWrite(NOTE6, mV);
  unsigned int velmV = map(voices[5].velocity, 0, 127, 0, 8191);
  analogWrite(VELOCITY6, velmV);
}

void updateVoice7() {
  unsigned int mV = (unsigned int)((float)(voices[6].note + transpose + realoctave) * NOTE_SF * sfAdj[6] + 0.5);
  analogWrite(NOTE7, mV);
  unsigned int velmV = map(voices[6].velocity, 0, 127, 0, 8191);
  analogWrite(VELOCITY7, velmV);
}

void updateVoice8() {
  unsigned int mV = (unsigned int)((float)(voices[7].note + transpose + realoctave) * NOTE_SF * sfAdj[7] + 0.5);
  analogWrite(NOTE8, mV);
  unsigned int velmV = map(voices[7].velocity, 0, 127, 0, 8191);
  analogWrite(VELOCITY8, velmV);
}


void updateUnisonCheck() {
  // if (digitalRead(UNISON_ON) == 1 && keyboardMode == 0)  //poly
  // {
  //   allNotesOff();
  //   keyboardMode = 3;
  // }

  // if (digitalRead(UNISON_ON) == 1 && (keyboardMode == 4 || keyboardMode == 5 || keyboardMode == 6))  // mono
  // {
  //   allNotesOff();
  //   keyboardMode = 3;
  // }

  // if (digitalRead(UNISON_ON) == 0 && (keyboardMode == 1 || keyboardMode == 2 || keyboardMode == 3))  //switch back from unison
  // {
  //   allNotesOff();
  //   keyboardMode = previousMode;
  // }
}

void allNotesOff() {
  sr.set(GATE_NOTE1, LOW);
  sr.set(GATE_NOTE2, LOW);
  sr.set(GATE_NOTE3, LOW);
  sr.set(GATE_NOTE4, LOW);
  sr.set(GATE_NOTE5, LOW);
  sr.set(GATE_NOTE6, LOW);
  sr.set(GATE_NOTE7, LOW);
  sr.set(GATE_NOTE8, LOW);

  sr.set(NOTE1_LED, LOW);
  sr.set(NOTE2_LED, LOW);
  sr.set(NOTE3_LED, LOW);
  sr.set(NOTE4_LED, LOW);
  sr.set(NOTE5_LED, LOW);
  sr.set(NOTE6_LED, LOW);
  sr.set(NOTE7_LED, LOW);
  sr.set(NOTE8_LED, LOW);

  voices[0].note = -1;
  voices[1].note = -1;
  voices[2].note = -1;
  voices[3].note = -1;
  voices[4].note = -1;
  voices[5].note = -1;
  voices[6].note = -1;
  voices[7].note = -1;

  voiceOn[0] = false;
  voiceOn[1] = false;
  voiceOn[2] = false;
  voiceOn[3] = false;
  voiceOn[4] = false;
  voiceOn[5] = false;
  voiceOn[6] = false;
  voiceOn[7] = false;
}

void setCurrentPatchData(String data[]) {
  patchName = data[0];
  polycount = data[1].toInt();
  channel1 = data[2].toInt();
  channel2 = data[3].toInt();
  channel3 = data[4].toInt();
  channel4 = data[5].toInt();
  channel5 = data[6].toInt();
  channel6 = data[7].toInt();
  channel7 = data[8].toInt();
  channel8 = data[9].toInt();
  channel9 = data[10].toInt();
  channel10 = data[11].toInt();
  channel10 = data[12].toInt();
  channel12 = data[13].toInt();
  channel13 = data[14].toInt();
  channel14 = data[15].toInt();
  channel15 = data[16].toInt();
  channel16 = data[17].toInt();

  gate1 = data[18].toInt();
  gate2 = data[19].toInt();
  gate3 = data[20].toInt();
  gate4 = data[21].toInt();
  gate5 = data[22].toInt();
  gate6 = data[23].toInt();
  gate7 = data[24].toInt();
  gate8 = data[25].toInt();

  keyboardMode = data[26].toInt();
  transpose = data[27].toInt();
  realoctave = data[28].toInt();

  channel1_CC = data[29].toInt();
  channel2_CC = data[30].toInt();
  channel3_CC = data[31].toInt();
  channel4_CC = data[32].toInt();
  channel5_CC = data[33].toInt();
  channel6_CC = data[34].toInt();
  channel7_CC = data[35].toInt();
  channel8_CC = data[36].toInt();
  channel9_CC = data[37].toInt();
  channel10_CC = data[38].toInt();
  channel10_CC = data[39].toInt();
  channel12_CC = data[40].toInt();
  channel13_CC = data[41].toInt();
  channel14_CC = data[42].toInt();
  channel15_CC = data[43].toInt();
  channel16_CC = data[44].toInt();

  channel1_MIDI = data[45].toInt();
  channel2_MIDI = data[46].toInt();
  channel3_MIDI = data[47].toInt();
  channel4_MIDI = data[48].toInt();
  channel5_MIDI = data[49].toInt();
  channel6_MIDI = data[50].toInt();
  channel7_MIDI = data[51].toInt();
  channel8_MIDI = data[52].toInt();
  channel9_MIDI = data[53].toInt();
  channel10_MIDI = data[54].toInt();
  channel10_MIDI = data[55].toInt();
  channel12_MIDI = data[56].toInt();
  channel13_MIDI = data[57].toInt();
  channel14_MIDI = data[58].toInt();
  channel15_MIDI = data[59].toInt();
  channel16_MIDI = data[60].toInt();

  //MUX2

  //Switches
  updatepolyCount();

  CC_MAP[0][0] = channel1_CC;
  CC_MAP[1][0] = channel2_CC;
  CC_MAP[2][0] = channel3_CC;
  CC_MAP[3][0] = channel5_CC;
  CC_MAP[4][0] = channel5_CC;
  CC_MAP[5][0] = channel6_CC;
  CC_MAP[6][0] = channel7_CC;
  CC_MAP[7][0] = channel8_CC;
  CC_MAP[8][0] = channel9_CC;
  CC_MAP[9][0] = channel10_CC;
  CC_MAP[10][0] = channel11_CC;
  CC_MAP[11][0] = channel12_CC;
  CC_MAP[12][0] = channel13_CC;
  CC_MAP[13][0] = channel14_CC;
  CC_MAP[14][0] = channel15_CC;
  CC_MAP[15][0] = channel16_CC;

  CC_MAP[0][1] = channel1_MIDI;
  CC_MAP[1][1] = channel2_MIDI;
  CC_MAP[2][1] = channel3_MIDI;
  CC_MAP[3][1] = channel5_MIDI;
  CC_MAP[4][1] = channel5_MIDI;
  CC_MAP[5][1] = channel6_MIDI;
  CC_MAP[6][1] = channel7_MIDI;
  CC_MAP[7][1] = channel8_MIDI;
  CC_MAP[8][1] = channel9_MIDI;
  CC_MAP[9][1] = channel10_MIDI;
  CC_MAP[10][1] = channel11_MIDI;
  CC_MAP[11][1] = channel12_MIDI;
  CC_MAP[12][1] = channel13_MIDI;
  CC_MAP[13][1] = channel14_MIDI;
  CC_MAP[14][1] = channel15_MIDI;
  CC_MAP[15][1] = channel16_MIDI;

  CC_MAP[0][4] = channel1;
  CC_MAP[1][4] = channel2;
  CC_MAP[2][4] = channel3;
  CC_MAP[3][4] = channel5;
  CC_MAP[4][4] = channel5;
  CC_MAP[5][4] = channel6;
  CC_MAP[6][4] = channel7;
  CC_MAP[7][4] = channel8;
  CC_MAP[8][4] = channel9;
  CC_MAP[9][4] = channel10;
  CC_MAP[10][4] = channel11;
  CC_MAP[11][4] = channel12;
  CC_MAP[12][4] = channel13;
  CC_MAP[13][4] = channel14;
  CC_MAP[14][4] = channel15;
  CC_MAP[15][4] = channel16;

  updateCCMAP();

  //Patchname
  updatePatchname();
}

String getCurrentPatchData() {
  return patchName + "," + String(polycount) + "," + String(channel1) + "," + String(channel2) + "," + String(channel3) + "," + String(channel4)
         + "," + String(channel5) + "," + String(channel6) + "," + String(channel7) + "," + String(channel8)
         + "," + String(channel9) + "," + String(channel10) + "," + String(channel11) + "," + String(channel12)
         + "," + String(channel13) + "," + String(channel14) + "," + String(channel15) + "," + String(channel16)
         + "," + String(gate1) + "," + String(gate2) + "," + String(gate3) + "," + String(gate4)
         + "," + String(gate5) + "," + String(gate6) + "," + String(gate7) + "," + String(gate8)
         + "," + String(keyboardMode) + "," + String(transpose) + "," + String(realoctave)
         + "," + String(channel1_CC) + "," + String(channel2_CC) + "," + String(channel3_CC) + "," + String(channel4_CC)
         + "," + String(channel5_CC) + "," + String(channel6_CC) + "," + String(channel7_CC) + "," + String(channel8_CC)
         + "," + String(channel9_CC) + "," + String(channel10_CC) + "," + String(channel11_CC) + "," + String(channel12_CC)
         + "," + String(channel13_CC) + "," + String(channel14_CC) + "," + String(channel15_CC) + "," + String(channel16_CC)
         + "," + String(channel1_MIDI) + "," + String(channel2_MIDI) + "," + String(channel3_MIDI) + "," + String(channel4_MIDI)
         + "," + String(channel5_MIDI) + "," + String(channel6_MIDI) + "," + String(channel7_MIDI) + "," + String(channel8_MIDI)
         + "," + String(channel9_MIDI) + "," + String(channel10_MIDI) + "," + String(channel11_MIDI) + "," + String(channel12_MIDI)
         + "," + String(channel13_MIDI) + "," + String(channel14_MIDI) + "," + String(channel15_MIDI) + "," + String(channel16_MIDI);
}

void updatePatchname() {
  showPatchPage(String(patchNo), patchName);
}

void updateCCMAP() {
  switch (polycount) {
    case 0:
      CC_MAP[0][2] = 0;
      CC_MAP[1][2] = 0;
      CC_MAP[2][2] = 0;
      CC_MAP[3][2] = 0;
      CC_MAP[4][2] = 0;
      CC_MAP[5][2] = 0;
      CC_MAP[6][2] = 0;
      CC_MAP[7][2] = 0;
      CC_MAP[8][2] = 0;
      CC_MAP[9][2] = 0;
      CC_MAP[10][2] = 0;
      CC_MAP[11][2] = 0;
      CC_MAP[12][2] = 0;
      CC_MAP[13][2] = 0;
      CC_MAP[14][2] = 0;
      CC_MAP[15][2] = 0;
      break;

    case 1:
      CC_MAP[0][2] = 1;
      CC_MAP[1][2] = 0;
      CC_MAP[2][2] = 0;
      CC_MAP[3][2] = 0;
      CC_MAP[4][2] = 0;
      CC_MAP[5][2] = 0;
      CC_MAP[6][2] = 0;
      CC_MAP[7][2] = 0;
      CC_MAP[8][2] = 1;
      CC_MAP[9][2] = 0;
      CC_MAP[10][2] = 0;
      CC_MAP[11][2] = 0;
      CC_MAP[12][2] = 0;
      CC_MAP[13][2] = 0;
      CC_MAP[14][2] = 0;
      CC_MAP[15][2] = 0;
      break;

    case 2:
      CC_MAP[0][2] = 1;
      CC_MAP[1][2] = 1;
      CC_MAP[2][2] = 0;
      CC_MAP[3][2] = 0;
      CC_MAP[4][2] = 0;
      CC_MAP[5][2] = 0;
      CC_MAP[6][2] = 0;
      CC_MAP[7][2] = 0;
      CC_MAP[8][2] = 1;
      CC_MAP[9][2] = 1;
      CC_MAP[10][2] = 0;
      CC_MAP[11][2] = 0;
      CC_MAP[12][2] = 0;
      CC_MAP[13][2] = 0;
      CC_MAP[14][2] = 0;
      CC_MAP[15][2] = 0;
      break;

    case 3:
      CC_MAP[0][2] = 1;
      CC_MAP[1][2] = 1;
      CC_MAP[2][2] = 1;
      CC_MAP[3][2] = 0;
      CC_MAP[4][2] = 0;
      CC_MAP[5][2] = 0;
      CC_MAP[6][2] = 0;
      CC_MAP[7][2] = 0;
      CC_MAP[8][2] = 1;
      CC_MAP[9][2] = 1;
      CC_MAP[10][2] = 1;
      CC_MAP[11][2] = 0;
      CC_MAP[12][2] = 0;
      CC_MAP[13][2] = 0;
      CC_MAP[14][2] = 0;
      CC_MAP[15][2] = 0;
      break;

    case 4:
      CC_MAP[0][2] = 1;
      CC_MAP[1][2] = 1;
      CC_MAP[2][2] = 1;
      CC_MAP[3][2] = 1;
      CC_MAP[4][2] = 0;
      CC_MAP[5][2] = 0;
      CC_MAP[6][2] = 0;
      CC_MAP[7][2] = 0;
      CC_MAP[8][2] = 1;
      CC_MAP[9][2] = 1;
      CC_MAP[10][2] = 1;
      CC_MAP[11][2] = 1;
      CC_MAP[12][2] = 0;
      CC_MAP[13][2] = 0;
      CC_MAP[14][2] = 0;
      CC_MAP[15][2] = 0;
      break;

    case 5:
      CC_MAP[0][2] = 1;
      CC_MAP[1][2] = 1;
      CC_MAP[2][2] = 1;
      CC_MAP[3][2] = 1;
      CC_MAP[4][2] = 1;
      CC_MAP[5][2] = 0;
      CC_MAP[6][2] = 0;
      CC_MAP[7][2] = 0;
      CC_MAP[8][2] = 1;
      CC_MAP[9][2] = 1;
      CC_MAP[10][2] = 1;
      CC_MAP[11][2] = 1;
      CC_MAP[12][2] = 1;
      CC_MAP[13][2] = 0;
      CC_MAP[14][2] = 0;
      CC_MAP[15][2] = 0;
      break;

    case 6:
      CC_MAP[0][2] = 1;
      CC_MAP[1][2] = 1;
      CC_MAP[2][2] = 1;
      CC_MAP[3][2] = 1;
      CC_MAP[4][2] = 1;
      CC_MAP[5][2] = 1;
      CC_MAP[6][2] = 0;
      CC_MAP[7][2] = 0;
      CC_MAP[8][2] = 1;
      CC_MAP[9][2] = 1;
      CC_MAP[10][2] = 1;
      CC_MAP[11][2] = 1;
      CC_MAP[12][2] = 1;
      CC_MAP[13][2] = 1;
      CC_MAP[14][2] = 0;
      CC_MAP[15][2] = 0;
      break;

    case 7:
      CC_MAP[0][2] = 1;
      CC_MAP[1][2] = 1;
      CC_MAP[2][2] = 1;
      CC_MAP[3][2] = 1;
      CC_MAP[4][2] = 1;
      CC_MAP[5][2] = 1;
      CC_MAP[6][2] = 1;
      CC_MAP[7][2] = 0;
      CC_MAP[8][2] = 1;
      CC_MAP[9][2] = 1;
      CC_MAP[10][2] = 1;
      CC_MAP[11][2] = 1;
      CC_MAP[12][2] = 1;
      CC_MAP[13][2] = 1;
      CC_MAP[14][2] = 1;
      CC_MAP[15][2] = 0;
      break;

    case 8:
      CC_MAP[0][2] = 1;
      CC_MAP[1][2] = 1;
      CC_MAP[2][2] = 1;
      CC_MAP[3][2] = 1;
      CC_MAP[4][2] = 1;
      CC_MAP[5][2] = 1;
      CC_MAP[6][2] = 1;
      CC_MAP[7][2] = 1;
      CC_MAP[8][2] = 1;
      CC_MAP[9][2] = 1;
      CC_MAP[10][2] = 1;
      CC_MAP[11][2] = 1;
      CC_MAP[12][2] = 1;
      CC_MAP[13][2] = 1;
      CC_MAP[14][2] = 1;
      CC_MAP[15][2] = 1;
      break;
  }
}

void checkeepromChanges() {

  if (oldeepromtranspose != eepromtranspose) {
    transpose = EEPROM.read(ADDR_TRANSPOSE);
    oldeepromtranspose = transpose;
    transpose = transpose - 12;
  }

  if (oldeepromOctave != eepromOctave) {
    octave = EEPROM.read(ADDR_OCTAVE);
    oldeepromOctave = octave;

    if (octave == 0) realoctave = -24;
    if (octave == 1) realoctave = -12;
    if (octave == 2) realoctave = 0;
    if (octave == 3) realoctave = 12;
    if (octave == 4) realoctave = 24;
  }
}


void recallPatch(int patchNo) {
  allNotesOff();
  File patchFile = SD.open(String(patchNo).c_str());
  if (!patchFile) {
    Serial.println("File not found");
  } else {
    String data[NO_OF_PARAMS];  //Array of data read in
    recallPatchData(patchFile, data);
    setCurrentPatchData(data);
    patchFile.close();
    storeLastPatch(patchNo);
  }
}

void showSettingsPage() {
  showSettingsPage(settings::current_setting(), settings::current_setting_value(), state);
}

void checkDrumEncoder() {

  long param_encRead = param_encoder.read();
  if ((param_encCW && param_encRead > param_encPrevious + 3) || (!param_encCW && param_encRead < param_encPrevious - 3)) {

    if (!paramEdit && !paramChange) {
      param_number = param_number + 1;
      if (param_number > 25) {
        param_number = 1;
      }
    }

    switch (param_number) {
      case 1:
        if (paramEdit) {
          polycount++;
          allNotesOff();
          if (polycount > 8) {
            polycount = 0;
          }
        }
        updatepolyCount();
        break;

      case 2:
        if (polycount > 0) {
          showCurrentParameterPage("Channel 1", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel1++;
          if (channel1 > CHANNEL_PARAMS) {
            channel1 = 0;
          }
          CC_MAP[0][4] = channel1;
        }
        if (paramChange && channel1 == 0) {
          channel1_CC++;
          if (channel1_CC > CHANNEL_CC_MAX) {
            channel1_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[0][0] = channel1_CC;
        }
        if (paramChange && channel1 == 1) {
          channel1_MIDI++;
          if (channel1_MIDI > CHANNEL_MIDI_MAX) {
            channel1_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[0][1] = channel1_MIDI;
        }

        updatechannel1();
        break;

      case 3:
        if (polycount > 1) {
          showCurrentParameterPage("Channel 2", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel2++;
          if (channel2 > CHANNEL_PARAMS) {
            channel2 = 0;
          }
          CC_MAP[1][4] = channel2;
        }
        if (paramChange && channel2 == 0) {
          channel2_CC++;
          if (channel2_CC > CHANNEL_CC_MAX) {
            channel2_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[1][0] = channel2_CC;
        }
        if (paramChange && channel2 == 1) {
          channel2_MIDI++;
          if (channel2_MIDI > CHANNEL_MIDI_MAX) {
            channel2_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[1][1] = channel2_MIDI;
        }
        updatechannel2();
        break;

      case 4:
        if (polycount > 2) {
          showCurrentParameterPage("Channel 3", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel3++;
          if (channel3 > CHANNEL_PARAMS) {
            channel3 = 0;
          }
          CC_MAP[2][4] = channel3;
        }
        if (paramChange && channel3 == 0) {
          channel3_CC++;
          if (channel3_CC > CHANNEL_CC_MAX) {
            channel3_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[2][0] = channel3_CC;
        }
        if (paramChange && channel3 == 1) {
          channel3_MIDI++;
          if (channel3_MIDI > CHANNEL_MIDI_MAX) {
            channel3_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[2][1] = channel3_MIDI;
        }
        updatechannel3();
        break;

      case 5:
        if (polycount > 3) {
          showCurrentParameterPage("Channel 4", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel4++;
          if (channel4 > CHANNEL_PARAMS) {
            channel4 = 0;
          }
          CC_MAP[3][4] = channel4;
        }
        if (paramChange && channel4 == 0) {
          channel4_CC++;
          if (channel4_CC > CHANNEL_CC_MAX) {
            channel4_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[3][0] = channel4_CC;
        }
        if (paramChange && channel4 == 1) {
          channel4_MIDI++;
          if (channel4_MIDI > CHANNEL_MIDI_MAX) {
            channel4_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[3][1] = channel4_MIDI;
        }
        updatechannel4();
        break;

      case 6:
        if (polycount > 4) {
          showCurrentParameterPage("Channel 5", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel5++;
          if (channel5 > CHANNEL_PARAMS) {
            channel5 = 0;
          }
          CC_MAP[4][4] = channel5;
        }
        if (paramChange && channel5 == 0) {
          channel5_CC++;
          if (channel5_CC > CHANNEL_CC_MAX) {
            channel5_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[4][0] = channel5_CC;
        }
        if (paramChange && channel5 == 1) {
          channel5_MIDI++;
          if (channel5_MIDI > CHANNEL_MIDI_MAX) {
            channel5_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[4][1] = channel5_MIDI;
        }
        updatechannel5();
        break;

      case 7:
        if (polycount > 5) {
          showCurrentParameterPage("Channel 6", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel6++;
          if (channel6 > CHANNEL_PARAMS) {
            channel6 = 0;
          }
          CC_MAP[5][4] = channel6;
        }
        if (paramChange && channel6 == 0) {
          channel6_CC++;
          if (channel6_CC > CHANNEL_CC_MAX) {
            channel6_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[5][0] = channel6_CC;
        }
        if (paramChange && channel6 == 1) {
          channel6_MIDI++;
          if (channel6_MIDI > CHANNEL_MIDI_MAX) {
            channel6_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[5][1] = channel6_MIDI;
        }
        updatechannel6();
        break;

      case 8:
        if (polycount > 6) {
          showCurrentParameterPage("Channel 7", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel7++;
          if (channel7 > CHANNEL_PARAMS) {
            channel7 = 0;
          }
          CC_MAP[6][4] = channel7;
        }
        if (paramChange && channel7 == 0) {
          channel7_CC++;
          if (channel7_CC > CHANNEL_CC_MAX) {
            channel7_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[6][0] = channel7_CC;
        }
        if (paramChange && channel7 == 1) {
          channel7_MIDI++;
          if (channel7_MIDI > CHANNEL_MIDI_MAX) {
            channel7_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[6][1] = channel7_MIDI;
        }
        updatechannel7();
        break;

      case 9:
        if (polycount > 7) {
          showCurrentParameterPage("Channel 8", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel8++;
          if (channel8 > CHANNEL_PARAMS) {
            channel8 = 0;
          }
          CC_MAP[7][4] = channel8;
        }
        if (paramChange && channel8 == 0) {
          channel8_CC++;
          if (channel8_CC > CHANNEL_CC_MAX) {
            channel8_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[7][0] = channel8_CC;
        }
        if (paramChange && channel8 == 1) {
          channel8_MIDI++;
          if (channel8_MIDI > CHANNEL_MIDI_MAX) {
            channel8_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[7][1] = channel8_MIDI;
        }
        updatechannel8();
        break;

      case 10:
        if (polycount > 0) {
          showCurrentParameterPage("Channel 9", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel9++;
          if (channel9 > CHANNEL_PARAMS) {
            channel9 = 0;
          }
          CC_MAP[8][4] = channel9;
        }
        if (paramChange && channel9 == 0) {
          channel9_CC++;
          if (channel9_CC > CHANNEL_CC_MAX) {
            channel9_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[8][0] = channel9_CC;
        }
        if (paramChange && channel9 == 1) {
          channel9_MIDI++;
          if (channel9_MIDI > CHANNEL_MIDI_MAX) {
            channel9_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[8][1] = channel9_MIDI;
        }
        updatechannel9();
        break;

      case 11:
        if (polycount > 1) {
          showCurrentParameterPage("Channel 10", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel10++;
          if (channel10 > CHANNEL_PARAMS) {
            channel10 = 0;
          }
          CC_MAP[9][4] = channel10;
        }
        if (paramChange && channel10 == 0) {
          channel10_CC++;
          if (channel10_CC > CHANNEL_CC_MAX) {
            channel10_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[9][0] = channel10_CC;
        }
        if (paramChange && channel10 == 1) {
          channel10_MIDI++;
          if (channel10_MIDI > CHANNEL_MIDI_MAX) {
            channel10_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[9][1] = channel10_MIDI;
        }
        updatechannel10();
        break;

      case 12:
        if (polycount > 2) {
          showCurrentParameterPage("Channel 11", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel11++;
          if (channel11 > CHANNEL_PARAMS) {
            channel11 = 0;
          }
          CC_MAP[10][4] = channel11;
        }
        if (paramChange && channel11 == 0) {
          channel11_CC++;
          if (channel11_CC > CHANNEL_CC_MAX) {
            channel1_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[10][0] = channel11_CC;
        }
        if (paramChange && channel11 == 1) {
          channel11_MIDI++;
          if (channel11_MIDI > CHANNEL_MIDI_MAX) {
            channel11_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[10][1] = channel11_MIDI;
        }
        updatechannel11();
        break;


      case 13:
        if (polycount > 3) {
          showCurrentParameterPage("Channel 12", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel12++;
          if (channel12 > CHANNEL_PARAMS) {
            channel12 = 0;
          }
          CC_MAP[11][4] = channel12;
        }
        if (paramChange && channel12 == 0) {
          channel12_CC++;
          if (channel12_CC > CHANNEL_CC_MAX) {
            channel12_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[11][0] = channel12_CC;
        }
        if (paramChange && channel12 == 1) {
          channel12_MIDI++;
          if (channel12_MIDI > CHANNEL_MIDI_MAX) {
            channel12_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[11][1] = channel12_MIDI;
        }
        updatechannel12();
        break;

      case 14:
        if (polycount > 4) {
          showCurrentParameterPage("Channel 13", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel13++;
          if (channel13 > CHANNEL_PARAMS) {
            channel13 = 0;
          }
          CC_MAP[12][4] = channel13;
        }
        if (paramChange && channel13 == 0) {
          channel13_CC++;
          if (channel13_CC > CHANNEL_CC_MAX) {
            channel13_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[12][0] = channel13_CC;
        }
        if (paramChange && channel13 == 1) {
          channel13_MIDI++;
          if (channel13_MIDI > CHANNEL_MIDI_MAX) {
            channel13_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[12][1] = channel13_MIDI;
        }
        updatechannel13();
        break;

      case 15:
        if (polycount > 5) {
          showCurrentParameterPage("Channel 14", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel14++;
          if (channel14 > CHANNEL_PARAMS) {
            channel14 = 0;
          }
          CC_MAP[13][4] = channel14;
        }
        if (paramChange && channel14 == 0) {
          channel14_CC++;
          if (channel14_CC > CHANNEL_CC_MAX) {
            channel14_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[13][0] = channel14_CC;
        }
        if (paramChange && channel1 == 14) {
          channel14_MIDI++;
          if (channel14_MIDI > CHANNEL_MIDI_MAX) {
            channel14_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[13][1] = channel14_MIDI;
        }
        updatechannel4();
        break;

      case 16:
        if (polycount > 6) {
          showCurrentParameterPage("Channel 15", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel15++;
          if (channel15 > CHANNEL_PARAMS) {
            channel15 = 0;
          }
          CC_MAP[14][4] = channel15;
        }
        if (paramChange && channel15 == 0) {
          channel15_CC++;
          if (channel15_CC > CHANNEL_CC_MAX) {
            channel15_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[14][0] = channel15_CC;
        }
        if (paramChange && channel15 == 1) {
          channel15_MIDI++;
          if (channel15_MIDI > CHANNEL_MIDI_MAX) {
            channel15_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[14][1] = channel15_MIDI;
        }
        updatechannel15();
        break;

      case 17:
        if (polycount > 7) {
          showCurrentParameterPage("Channel 16", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel16++;
          if (channel16 > CHANNEL_PARAMS) {
            channel16 = 0;
          }
          CC_MAP[15][4] = channel16;
        }
        if (paramChange && channel16 == 0) {
          channel16_CC++;
          if (channel16_CC > CHANNEL_CC_MAX) {
            channel16_CC = CHANNEL_CC_MIN;
          }
          CC_MAP[15][0] = channel16_CC;
        }
        if (paramChange && channel16 == 1) {
          channel16_MIDI++;
          if (channel16_MIDI > CHANNEL_MIDI_MAX) {
            channel16_MIDI = CHANNEL_MIDI_MIN;
          }
          CC_MAP[15][1] = channel16_MIDI;
        }
        updatechannel16();
        break;

      case 18:
        if (polycount > 0) {
          showCurrentParameterPage("Gate 1", "Gate Mode");
          break;
        }
        if (gate1 < 36) gate1 = 36;
        if (paramEdit) {
          gate1++;
          if (gate1 > GATE_PARAMS) {
            gate1 = 36;
          }
        }
        updategate1();
        break;

      case 19:
        if (polycount > 1) {
          showCurrentParameterPage("Gate 2", "Gate Mode");
          break;
        }
        if (gate2 < 36) gate2 = 36;
        if (paramEdit) {
          gate2++;
          if (gate2 > GATE_PARAMS) {
            gate2 = 36;
          }
        }
        updategate2();
        break;

      case 20:
        if (polycount > 2) {
          showCurrentParameterPage("Gate 3", "Gate Mode");
          break;
        }
        if (gate3 < 36) gate3 = 36;
        if (paramEdit) {
          gate3++;
          if (gate3 > GATE_PARAMS) {
            gate3 = 36;
          }
        }
        updategate3();
        break;

      case 21:
        if (polycount > 3) {
          showCurrentParameterPage("Gate 4", "Gate Mode");
          break;
        }
        if (gate4 < 36) gate4 = 36;
        if (paramEdit) {
          gate4++;
          if (gate4 > GATE_PARAMS) {
            gate4 = 36;
          }
        }
        updategate4();
        break;

      case 22:
        if (polycount > 4) {
          showCurrentParameterPage("Gate 5", "Gate Mode");
          break;
        }
        if (gate5 < 36) gate5 = 36;
        if (paramEdit) {
          gate5++;
          if (gate5 > GATE_PARAMS) {
            gate5 = 36;
          }
        }
        updategate5();
        break;

      case 23:
        if (polycount > 5) {
          showCurrentParameterPage("Gate 6", "Gate Mode");
          break;
        }
        if (gate6 < 36) gate6 = 36;
        if (paramEdit) {
          gate6++;
          if (gate6 > GATE_PARAMS) {
            gate6 = 36;
          }
        }
        updategate6();
        break;

      case 24:
        if (polycount > 6) {
          showCurrentParameterPage("Gate 7", "Gate Mode");
          break;
        }
        if (gate7 < 36) gate7 = 36;
        if (paramEdit) {
          gate7++;
          if (gate7 > GATE_PARAMS) {
            gate7 = 36;
          }
        }
        updategate7();
        break;

      case 25:
        if (polycount > 7) {
          showCurrentParameterPage("Gate 8", "Gate Mode");
          break;
        }
        if (gate8 < 36) gate8 = 36;
        if (paramEdit) {
          gate8++;
          if (gate8 > GATE_PARAMS) {
            gate8 = 36;
          }
        }
        updategate8();
        break;
    }

    param_encPrevious = param_encRead;

  } else if ((param_encCW && param_encRead < param_encPrevious - 3) || (!param_encCW && param_encRead > param_encPrevious + 3)) {

    if (!paramEdit && !paramChange) {
      param_number = param_number - 1;
      if (param_number < 1) {
        param_number = 25;
      }
    }
    switch (param_number) {
      case 1:
        if (paramEdit) {
          polycount--;
          allNotesOff();
          if (polycount < 0) {
            polycount = 8;
          }
        }
        updatepolyCount();
        break;

      case 2:
        if (polycount > 0) {
          showCurrentParameterPage("Channel 1", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel1--;
          if (channel1 < 0) {
            channel1 = CHANNEL_PARAMS;
          }
          CC_MAP[0][4] = channel1;
        }
        if (paramChange && channel1 == 0) {
          channel1_CC--;
          if (channel1_CC < CHANNEL_CC_MIN) {
            channel1_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[0][0] = channel1_CC;
        }
        if (paramChange && channel1 == 1) {
          channel1_MIDI--;
          if (channel1_MIDI < CHANNEL_MIDI_MIN) {
            channel1_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[0][1] = channel1_MIDI;
        }
        updatechannel1();
        break;

      case 3:
        if (polycount > 1) {
          showCurrentParameterPage("Channel 2", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel2--;
          if (channel2 < 0) {
            channel2 = CHANNEL_PARAMS;
          }
          CC_MAP[1][4] = channel2;
        }
        if (paramChange && channel2 == 0) {
          channel2_CC--;
          if (channel2_CC < CHANNEL_CC_MIN) {
            channel2_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[1][0] = channel2_CC;
        }
        if (paramChange && channel2 == 1) {
          channel2_MIDI--;
          if (channel2_MIDI < CHANNEL_MIDI_MIN) {
            channel2_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[1][1] = channel2_MIDI;
        }
        updatechannel2();
        break;

      case 4:
        if (polycount > 2) {
          showCurrentParameterPage("Channel 3", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel3--;
          if (channel3 < 0) {
            channel3 = CHANNEL_PARAMS;
          }
          CC_MAP[2][4] = channel3;
        }
        if (paramChange && channel3 == 0) {
          channel3_CC--;
          if (channel3_CC < CHANNEL_CC_MIN) {
            channel3_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[2][0] = channel3_CC;
        }
        if (paramChange && channel3 == 1) {
          channel3_MIDI--;
          if (channel3_MIDI < CHANNEL_MIDI_MIN) {
            channel3_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[2][1] = channel3_MIDI;
        }
        updatechannel3();
        break;

      case 5:
        if (polycount > 3) {
          showCurrentParameterPage("Channel 4", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel4--;
          if (channel4 < 0) {
            channel4 = CHANNEL_PARAMS;
          }
          CC_MAP[3][4] = channel4;
        }
        if (paramChange && channel4 == 0) {
          channel4_CC--;
          if (channel4_CC < CHANNEL_CC_MIN) {
            channel4_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[3][0] = channel4_CC;
        }
        if (paramChange && channel4 == 1) {
          channel4_MIDI--;
          if (channel4_MIDI < CHANNEL_MIDI_MIN) {
            channel4_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[3][1] = channel4_MIDI;
        }
        updatechannel4();
        break;

      case 6:
        if (polycount > 4) {
          showCurrentParameterPage("Channel 5", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel5--;
          if (channel5 < 0) {
            channel5 = CHANNEL_PARAMS;
          }
          CC_MAP[4][4] = channel5;
        }
        if (paramChange && channel5 == 0) {
          channel5_CC--;
          if (channel5_CC < CHANNEL_CC_MIN) {
            channel5_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[4][0] = channel5_CC;
        }
        if (paramChange && channel5 == 1) {
          channel5_MIDI--;
          if (channel5_MIDI < CHANNEL_MIDI_MIN) {
            channel5_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[4][1] = channel5_MIDI;
        }
        updatechannel5();
        break;

      case 7:
        if (polycount > 5) {
          showCurrentParameterPage("Channel 6", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel6--;
          if (channel6 < 0) {
            channel6 = CHANNEL_PARAMS;
          }
          CC_MAP[5][4] = channel6;
        }
        if (paramChange && channel6 == 0) {
          channel6_CC--;
          if (channel6_CC < CHANNEL_CC_MIN) {
            channel6_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[5][0] = channel6_CC;
        }
        if (paramChange && channel6 == 1) {
          channel6_MIDI--;
          if (channel6_MIDI < CHANNEL_MIDI_MIN) {
            channel6_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[5][1] = channel6_MIDI;
        }
        updatechannel6();
        break;

      case 8:
        if (polycount > 6) {
          showCurrentParameterPage("Channel 7", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel7--;
          if (channel7 < 0) {
            channel7 = CHANNEL_PARAMS;
          }
          CC_MAP[6][4] = channel7;
        }
        if (paramChange && channel7 == 0) {
          channel7_CC--;
          if (channel7_CC < CHANNEL_CC_MIN) {
            channel7_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[6][0] = channel7_CC;
        }
        if (paramChange && channel7 == 1) {
          channel7_MIDI--;
          if (channel7_MIDI < CHANNEL_MIDI_MIN) {
            channel7_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[6][1] = channel7_MIDI;
        }
        updatechannel7();
        break;

      case 9:
        if (polycount > 7) {
          showCurrentParameterPage("Channel 8", "Poly Mode");
          break;
        }
        if (paramEdit) {
          channel8--;
          if (channel8 < 0) {
            channel8 = CHANNEL_PARAMS;
          }
          CC_MAP[7][4] = channel8;
        }
        if (paramChange && channel8 == 0) {
          channel8_CC--;
          if (channel8_CC < CHANNEL_CC_MIN) {
            channel8_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[7][0] = channel8_CC;
        }
        if (paramChange && channel8 == 1) {
          channel8_MIDI--;
          if (channel8_MIDI < CHANNEL_MIDI_MIN) {
            channel8_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[7][1] = channel8_MIDI;
        }
        updatechannel8();
        break;

      case 10:
        if (polycount > 0) {
          showCurrentParameterPage("Channel 9", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel9--;
          if (channel9 < 0) {
            channel9 = CHANNEL_PARAMS;
          }
          CC_MAP[8][4] = channel9;
        }
        if (paramChange && channel9 == 0) {
          channel9_CC--;
          if (channel9_CC < CHANNEL_CC_MIN) {
            channel9_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[8][0] = channel9_CC;
        }
        if (paramChange && channel9 == 1) {
          channel9_MIDI--;
          if (channel9_MIDI < CHANNEL_MIDI_MIN) {
            channel9_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[8][1] = channel9_MIDI;
        }
        updatechannel9();
        break;

      case 11:
        if (polycount > 1) {
          showCurrentParameterPage("Channel 10", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel10--;
          if (channel10 < 0) {
            channel10 = CHANNEL_PARAMS;
          }
          CC_MAP[9][4] = channel10;
        }
        if (paramChange && channel10 == 0) {
          channel10_CC--;
          if (channel10_CC < CHANNEL_CC_MIN) {
            channel10_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[9][0] = channel10_CC;
        }
        if (paramChange && channel10 == 1) {
          channel10_MIDI--;
          if (channel10_MIDI < CHANNEL_MIDI_MIN) {
            channel10_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[9][1] = channel10_MIDI;
        }
        updatechannel10();
        break;

      case 12:
        if (polycount > 2) {
          showCurrentParameterPage("Channel 11", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel11--;
          if (channel11 < 0) {
            channel11 = CHANNEL_PARAMS;
          }
          CC_MAP[10][4] = channel11;
        }
        if (paramChange && channel11 == 0) {
          channel11_CC--;
          if (channel11_CC < CHANNEL_CC_MIN) {
            channel11_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[10][0] = channel11_CC;
        }
        if (paramChange && channel11 == 1) {
          channel11_MIDI--;
          if (channel11_MIDI < CHANNEL_MIDI_MIN) {
            channel11_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[10][1] = channel11_MIDI;
        }
        updatechannel11();
        break;

      case 13:
        if (polycount > 3) {
          showCurrentParameterPage("Channel 12", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel12--;
          if (channel12 < 0) {
            channel12 = CHANNEL_PARAMS;
          }
          CC_MAP[11][4] = channel12;
        }
        if (paramChange && channel12 == 0) {
          channel12_CC--;
          if (channel12_CC < CHANNEL_CC_MIN) {
            channel12_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[11][0] = channel12_CC;
        }
        if (paramChange && channel12 == 1) {
          channel12_MIDI--;
          if (channel12_MIDI < CHANNEL_MIDI_MIN) {
            channel12_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[11][1] = channel12_MIDI;
        }
        updatechannel12();
        break;

      case 14:
        if (polycount > 4) {
          showCurrentParameterPage("Channel 13", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel13--;
          if (channel13 < 0) {
            channel13 = CHANNEL_PARAMS;
          }
          CC_MAP[12][4] = channel13;
        }
        if (paramChange && channel13 == 0) {
          channel13_CC--;
          if (channel13_CC < CHANNEL_CC_MIN) {
            channel13_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[12][0] = channel13_CC;
        }
        if (paramChange && channel13 == 1) {
          channel13_MIDI--;
          if (channel13_MIDI < CHANNEL_MIDI_MIN) {
            channel13_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[12][1] = channel13_MIDI;
        }
        updatechannel13();
        break;

      case 15:
        if (polycount > 5) {
          showCurrentParameterPage("Channel 14", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel14--;
          if (channel14 < 0) {
            channel14 = CHANNEL_PARAMS;
          }
          CC_MAP[13][4] = channel14;
        }
        if (paramChange && channel14 == 0) {
          channel14_CC--;
          if (channel14_CC < CHANNEL_CC_MIN) {
            channel14_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[13][0] = channel14_CC;
        }
        if (paramChange && channel14 == 1) {
          channel14_MIDI--;
          if (channel14_MIDI < CHANNEL_MIDI_MIN) {
            channel14_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[13][1] = channel14_MIDI;
        }
        updatechannel14();
        break;

      case 16:
        if (polycount > 6) {
          showCurrentParameterPage("Channel 15", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel15--;
          if (channel15 < 0) {
            channel15 = CHANNEL_PARAMS;
          }
          CC_MAP[14][4] = channel15;
        }
        if (paramChange && channel15 == 0) {
          channel15_CC--;
          if (channel15_CC < CHANNEL_CC_MIN) {
            channel15_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[14][0] = channel15_CC;
        }
        if (paramChange && channel15 == 1) {
          channel15_MIDI--;
          if (channel15_MIDI < CHANNEL_MIDI_MIN) {
            channel15_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[14][1] = channel15_MIDI;
        }
        updatechannel15();
        break;

      case 17:
        if (polycount > 7) {
          showCurrentParameterPage("Channel 16", "Velocity Mode");
          break;
        }
        if (paramEdit) {
          channel16--;
          if (channel16 < 36) {
            channel16 = CHANNEL_PARAMS;
          }
          CC_MAP[15][4] = channel16;
        }
        if (paramChange && channel16 == 0) {
          channel16_CC--;
          if (channel16_CC < CHANNEL_CC_MIN) {
            channel16_CC = CHANNEL_CC_MAX;
          }
          CC_MAP[15][0] = channel16_CC;
        }
        if (paramChange && channel16 == 1) {
          channel16_MIDI--;
          if (channel16_MIDI < CHANNEL_MIDI_MIN) {
            channel16_MIDI = CHANNEL_MIDI_MAX;
          }
          CC_MAP[15][1] = channel16_MIDI;
        }
        updatechannel16();
        break;

      case 18:
        if (polycount > 0) {
          showCurrentParameterPage("Gate 1", "Gate Mode");
          break;
        }
        if (paramEdit) {
          gate1--;
          if (gate1 < 36) {
            gate1 = GATE_PARAMS;
          }
        }
        updategate1();
        break;

      case 19:
        if (polycount > 1) {
          showCurrentParameterPage("Gate 2", "Gate Mode");
          break;
        }
        if (paramEdit) {
          gate2--;
          if (gate2 < 36) {
            gate2 = GATE_PARAMS;
          }
        }
        updategate2();
        break;

      case 20:
        if (polycount > 2) {
          showCurrentParameterPage("Gate 3", "Gate Mode");
          break;
        }
        if (paramEdit) {
          gate3--;
          if (gate3 < 36) {
            gate3 = GATE_PARAMS;
          }
        }
        updategate3();
        break;

      case 21:
        if (polycount > 3) {
          showCurrentParameterPage("Gate 4", "Gate Mode");
          break;
        }
        if (paramEdit) {
          gate4--;
          if (gate4 < 36) {
            gate4 = GATE_PARAMS;
          }
        }
        updategate4();
        break;

      case 22:
        if (polycount > 4) {
          showCurrentParameterPage("Gate 5", "Gate Mode");
          break;
        }
        if (paramEdit) {
          gate5--;
          if (gate5 < 36) {
            gate5 = GATE_PARAMS;
          }
        }
        updategate5();
        break;

      case 23:
        if (polycount > 5) {
          showCurrentParameterPage("Gate 6", "Gate Mode");
          break;
        }
        if (paramEdit) {
          gate6--;
          if (gate6 < 36) {
            gate6 = GATE_PARAMS;
          }
        }
        updategate6();
        break;

      case 24:
        if (polycount > 6) {
          showCurrentParameterPage("Gate 7", "Gate Mode");
          break;
        }
        if (paramEdit) {
          gate7--;
          if (gate7 < 36) {
            gate7 = GATE_PARAMS;
          }
        }
        updategate7();
        break;

      case 25:
        if (polycount > 7) {
          showCurrentParameterPage("Gate 8", "Gate Mode");
          break;
        }
        if (paramEdit) {
          gate8--;
          if (gate8 < 36) {
            gate8 = GATE_PARAMS;
          }
        }
        updategate8();
        break;
    }
    param_encPrevious = param_encRead;
  }
}

void checkSwitches() {

  paramButton.update(digitalRead(PARAM_SW), 50, LOW);
  if (paramButton.held()) {
    paramChange = !paramChange;
    if (paramChange) {
      paramEdit = false;
    }
    if (!paramChange) {
      paramEdit = false;
    }
  } else if (paramButton.released(true)) {
    paramEdit = !paramEdit;
  }

  saveButton.update();
  if (saveButton.held()) {
    switch (state) {
      case PARAMETER:
      case PATCH:
        state = DELETE;
        break;
    }
  } else if (saveButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        if (patches.size() < PATCHES_LIMIT) {
          resetPatchesOrdering();  //Reset order of patches from first patch
          patches.push({ patches.size() + 1, INITPATCHNAME });
          state = SAVE;
        }
        break;
      case SAVE:
        //Save as new patch with INITIALPATCH name or overwrite existing keeping name - bypassing patch renaming
        patchName = patches.last().patchName;
        state = PATCH;
        savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
        showPatchPage(patches.last().patchNo, patches.last().patchName);
        patchNo = patches.last().patchNo;
        loadPatches();  //Get rid of pushed patch if it wasn't saved
        setPatchesOrdering(patchNo);
        renamedPatch = "";
        state = PARAMETER;
        break;
      case PATCHNAMING:
        if (renamedPatch.length() > 0) patchName = renamedPatch;  //Prevent empty strings
        state = PATCH;
        savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
        showPatchPage(patches.last().patchNo, patchName);
        patchNo = patches.last().patchNo;
        loadPatches();  //Get rid of pushed patch if it wasn't saved
        setPatchesOrdering(patchNo);
        renamedPatch = "";
        state = PARAMETER;
        break;
    }
  }

  settingsButton.update();
  if (settingsButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = SETTINGS;
        showSettingsPage();
        break;
      case SETTINGS:
        showSettingsPage();
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }

  backButton.update();
  if (backButton.held()) {
    //If Back button held, Panic - all notes off
    allNotesOff();                           //Come out of this state
  } else if (backButton.numClicks() == 1) {  //cannot be fallingEdge because holding button won't work
    switch (state) {
      case RECALL:
        setPatchesOrdering(patchNo);
        state = PARAMETER;
        break;
      case SAVE:
        renamedPatch = "";
        state = PARAMETER;
        loadPatches();  //Remove patch that was to be saved
        setPatchesOrdering(patchNo);
        break;
      case PATCHNAMING:
        charIndex = 0;
        renamedPatch = "";
        state = SAVE;
        break;
      case DELETE:
        setPatchesOrdering(patchNo);
        state = PARAMETER;
        break;
      case SETTINGS:
        state = PARAMETER;
        break;
      case SETTINGSVALUE:
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }

  //Encoder switch
  recallButton.update();
  if (recallButton.held()) {
    //If Recall button held, return to current patch setting
    //which clears any changes made
    state = PATCH;
    //Recall the current patch
    patchNo = patches.first().patchNo;
    recallPatch(patchNo);
    state = PARAMETER;
  } else if (recallButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = RECALL;  //show patch list
        break;
      case RECALL:
        state = PATCH;
        //Recall the current patch
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case SAVE:
        showRenamingPage(patches.last().patchName);
        patchName = patches.last().patchName;
        state = PATCHNAMING;
        break;
      case PATCHNAMING:
        if (renamedPatch.length() < 13) {
          renamedPatch.concat(String(currentCharacter));
          charIndex = 0;
          currentCharacter = CHARACTERS[charIndex];
          showRenamingPage(renamedPatch);
        }
        break;
      case DELETE:
        //Don't delete final patch
        if (patches.size() > 1) {
          state = DELETEMSG;
          patchNo = patches.first().patchNo;     //PatchNo to delete from SD card
          patches.shift();                       //Remove patch from circular buffer
          deletePatch(String(patchNo).c_str());  //Delete from SD card
          loadPatches();                         //Repopulate circular buffer to start from lowest Patch No
          renumberPatchesOnSD();
          loadPatches();                      //Repopulate circular buffer again after delete
          patchNo = patches.first().patchNo;  //Go back to 1
          recallPatch(patchNo);               //Load first patch
        }
        state = PARAMETER;
        break;
      case SETTINGS:
        state = SETTINGSVALUE;
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }
}

void checkEncoder() {
  //Encoder works with relative inc and dec values
  //Detent encoder goes up in 4 steps, hence +/-3

  long encRead = encoder.read();
  if ((encCW && encRead > encPrevious + 3) || (!encCW && encRead < encPrevious - 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.push(patches.shift());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case RECALL:
        patches.push(patches.shift());
        break;
      case SAVE:
        patches.push(patches.shift());
        break;
      case PATCHNAMING:
        if (charIndex == TOTALCHARS) charIndex = 0;  //Wrap around
        currentCharacter = CHARACTERS[charIndex++];
        showRenamingPage(renamedPatch + currentCharacter);
        break;
      case DELETE:
        patches.push(patches.shift());
        break;
      case SETTINGS:
        settings::increment_setting();
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::increment_setting_value();
        showSettingsPage();
        break;
    }
    encPrevious = encRead;
  } else if ((encCW && encRead < encPrevious - 3) || (!encCW && encRead > encPrevious + 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.unshift(patches.pop());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case RECALL:
        patches.unshift(patches.pop());
        break;
      case SAVE:
        patches.unshift(patches.pop());
        break;
      case PATCHNAMING:
        if (charIndex == -1)
          charIndex = TOTALCHARS - 1;
        currentCharacter = CHARACTERS[charIndex--];
        showRenamingPage(renamedPatch + currentCharacter);
        break;
      case DELETE:
        patches.unshift(patches.pop());
        break;
      case SETTINGS:
        settings::decrement_setting();
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::decrement_setting_value();
        showSettingsPage();
        break;
    }
    encPrevious = encRead;
  }
}

void loop() {

  checkSwitches();
  checkDrumEncoder();
  checkeepromChanges();
  checkEncoder();
  myusb.Task();
  midi1.read(0);    //USB HOST MIDI Class Compliant
  MIDI.read(0);     //MIDI 5 Pin DIN
  usbMIDI.read(0);  //USB Client MIDI
  ledsOff();
}

void ledsOff() {

  if ((clock_timer > 0) && (millis() - clock_timer > 60)) {
    sr.set(CLOCK_LED, LOW);
    clock_timer = 0;
  }

  if ((pitchbend_timer > 0) && (millis() - pitchbend_timer > 60)) {
    sr.set(PITCHBEND_LED, LOW);
    pitchbend_timer = 0;
  }

  if ((mod_timer > 0) && (millis() - mod_timer > 60)) {
    sr.set(MOD_LED, LOW);
    mod_timer = 0;
  }

  if ((aftertouch_timer > 0) && (millis() - aftertouch_timer > 60)) {
    sr.set(AFTERTOUCH_LED, LOW);
    aftertouch_timer = 0;
  }

  if ((breath_timer > 0) && (millis() - breath_timer > 60)) {
    sr.set(BREATH_LED, LOW);
    breath_timer = 0;
  }

  if ((reset_timer > 0) && (millis() - reset_timer > 60)) {
    sr.set(RESET, LOW);
    reset_timer = 0;
  }

  for (int i = 0; i < 16; i++) {
    if ((CC_MAP[i][2] == 0 && CC_MAP[i][4] == 2) || (CC_MAP[i][2] == 0 && CC_MAP[i][4] == 3) || (CC_MAP[i][2] == 0 && CC_MAP[i][4] == 4) || (CC_MAP[i][2] == 0 && CC_MAP[i][4] == 5)) {
      if ((outputLEDS[i] > 0) && (millis() - outputLEDS[i] > 60)) {
        sr.set(CC_MAP[i][5], LOW);
        outputLEDS[i] = 0;
      }
    }
  }
}



//     case SCALE_FACTOR_SET_CH:
//       if ((encoderPos > encoderPosPrev) && (sfAdj[setCh] < 1.1))
//         sfAdj[setCh] += 0.001f;
//       else if ((encoderPos < encoderPosPrev) && (sfAdj[setCh] > 0.9))
//         sfAdj[setCh] -= 0.001f;

//     case SCALE_FACTOR:
//       display.setCursor(0, 0);
//       display.setTextColor(WHITE, BLACK);
//       display.print(F("SET SCALE FACTOR"));
//       display.setCursor(0, 8);

//       if (menu == SCALE_FACTOR) setHighlight(0, 7);
//       display.print(F("Note 1:    "));
//       if ((menu == SCALE_FACTOR_SET_CH) && setCh == 0) display.setTextColor(BLACK, WHITE);
//       display.println(sfAdj[0], 3);

//       if (menu == SCALE_FACTOR) setHighlight(1, 7);
//       else display.setTextColor(WHITE, BLACK);
//       display.print(F("Note 2:    "));
//       if ((menu == SCALE_FACTOR_SET_CH) && setCh == 1) display.setTextColor(BLACK, WHITE);
//       display.println(sfAdj[1], 3);

//       if (menu == SCALE_FACTOR) setHighlight(2, 7);
//       else display.setTextColor(WHITE, BLACK);
//       display.print(F("Note 3:    "));
//       if ((menu == SCALE_FACTOR_SET_CH) && setCh == 2) display.setTextColor(BLACK, WHITE);
//       display.println(sfAdj[2], 3);

//       if (menu == SCALE_FACTOR) setHighlight(3, 7);
//       else display.setTextColor(WHITE, BLACK);
//       display.print(F("Note 4:    "));
//       if ((menu == SCALE_FACTOR_SET_CH) && setCh == 3) display.setTextColor(BLACK, WHITE);
//       display.println(sfAdj[3], 3);

//       if (menu == SCALE_FACTOR) setHighlight(4, 7);
//       else display.setTextColor(WHITE, BLACK);
//       display.print(F("Note 5:    "));
//       if ((menu == SCALE_FACTOR_SET_CH) && setCh == 4) display.setTextColor(BLACK, WHITE);
//       display.println(sfAdj[4], 3);

//       if (menu == SCALE_FACTOR) setHighlight(5, 7);
//       else display.setTextColor(WHITE, BLACK);
//       display.print(F("Note 6:    "));
//       if ((menu == SCALE_FACTOR_SET_CH) && setCh == 5) display.setTextColor(BLACK, WHITE);
//       display.println(sfAdj[5], 3);

//       if (menu == SCALE_FACTOR) setHighlight(6, 7);
//       else display.setTextColor(WHITE, BLACK);
//       display.print(F("Return      "));
//   }
//   display.display();
// }

// void setHighlight(int menuItem, int numMenuItems) {
//   if ((mod(encoderPos, numMenuItems) == menuItem) && highlightEnabled) {
//     display.setTextColor(BLACK, WHITE);
//   } else {
//     display.setTextColor(WHITE, BLACK);
//   }
// }

int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}
