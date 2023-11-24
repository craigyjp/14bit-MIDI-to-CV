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
#include <Wire.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MIDI.h>
#include "MidiCC.h"
#include "Constants.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include <USBHost_t36.h>
#include "HWControls.h"
#include "EepromMgr.h"
#include "Settings.h"
#include <RoxMux.h>


// OLED I2C is used on pins 18 and 19 for Teensy 3.x

// Voices available
#define NO_OF_VOICES 8

// 10 octave keyboard on a 3.3v powered PWM output scaled to 10V

#define NOTE_SF 129.5f

#define PARAMETER 0      //The main page for displaying the current patch and control (parameter) changes
#define RECALL 1         //Patches list
#define SAVE 2           //Save patch page
#define REINITIALISE 3   // Reinitialise message
#define PATCH 4          // Show current patch bypassing PARAMETER
#define PATCHNAMING 5    // Patch naming page
#define DELETE 6         //Delete patch page
#define DELETEMSG 7      //Delete patch message page
#define SETTINGS 8       //Settings page
#define SETTINGSVALUE 9  //Settings page

unsigned int state = PARAMETER;

#include "SSD1306Display.h"

boolean cardStatus = false;

float sfAdj[8];

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
bool S1, S2;

// MIDI setup

//USB HOST MIDI Class Compliant
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
const int channel = 1;

int patchNo = 1;  //Current patch no

#define MUX_TOTAL 4
Rox74HC595<MUX_TOTAL> sr;
#define PIN_DATA 30   // pin 14 on 74HC595 (DATA)
#define PIN_LATCH 32  // pin 12 on 74HC595 (LATCH)
#define PIN_CLK 31    // pin 11 on 74HC595 (CLK)
#define PIN_PWM -1    // pin 13 on 74HC595

void setup() {

  sr.begin(PIN_DATA, PIN_LATCH, PIN_CLK, PIN_PWM);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // OLED I2C Address, may need to change for different device,
  setupDisplay();
  setUpSettings();
  setupHardware();

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
  MIDI.begin(midiChannel);
  MIDI.setHandleNoteOn(myNoteOn);
  MIDI.setHandleNoteOff(myNoteOff);
  MIDI.setHandlePitchBend(myPitchBend);
  MIDI.setHandleControlChange(myControlChange);
  MIDI.setHandleAfterTouchChannel(myAfterTouch);
  Serial.println("MIDI In DIN Listening");

  //USB Client MIDI
  usbMIDI.setHandleControlChange(myControlChange);
  usbMIDI.setHandleNoteOff(myNoteOff);
  usbMIDI.setHandleNoteOn(myNoteOn);
  usbMIDI.setHandlePitchChange(myPitchBend);
  usbMIDI.setHandleAfterTouchChannel(myAfterTouch);
  Serial.println("USB Client MIDI Listening");

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // OLED I2C Address, may need to change for different device,
  // Check with I2C_Scanner

  // Wire.setClock(100000L);  // Uncomment to slow down I2C speed

  // Read Settings from EEPROM
  for (int i = 0; i < 8; i++) {
    EEPROM.get(ADDR_SF_ADJUST + i * sizeof(float), sfAdj[i]);
    if ((sfAdj[i] < 0.9f) || (sfAdj[i] > 1.1f) || isnan(sfAdj[i])) sfAdj[i] = 1.0f;
  }

  // Keyboard mode
  keyboardMode = EEPROM.read(ADDR_KEYBOARD_MODE);

  // MIDI Channel
  midiChannel = EEPROM.read(EEPROM_MIDI_CH);

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

  analogWrite(PITCHBEND, 1543);

  recallPatch(1);
}

void myPitchBend(byte channel, int bend) {
  if (channel == midiChannel) {
    int newbend = map(bend, -8191, 8192, 0, 3087);
    analogWrite(PITCHBEND, newbend);
  }
}

void myControlChange(byte channel, byte number, byte value) {
  if (channel == midiChannel) {
    if (number == 1) {
      int newvalue = value;
      newvalue = map(newvalue, 0, 127, 0, 7720);
      analogWrite(WHEEL, newvalue);
    }

    if (number == 2) {
      int newvalue = value;
      newvalue = map(newvalue, 0, 127, 0, 7720);
      analogWrite(BREATH, newvalue);
    }
  }
}

void myAfterTouch(byte channel, byte value) {
  if (channel == midiChannel) {
    int newvalue = value;
    newvalue = map(newvalue, 0, 127, 0, 7720);
    analogWrite(AFTERTOUCH, newvalue);
  }
}

void updatepolyCount() {
  showCurrentParameterPage("Poly Count", String(polycount) + " Notes");
}

void updatechannel1() {
  showCurrentParameterPage("Channel 1", channel1);
}

void updatechannel2() {
  showCurrentParameterPage("Channel 2", channel2);
}

void updatechannel3() {
  showCurrentParameterPage("Channel 3", channel3);
}

void updatechannel4() {
  showCurrentParameterPage("Channel 4", channel4);
}

void updatechannel5() {
  showCurrentParameterPage("Channel 5", channel5);
}

void updatechannel6() {
  showCurrentParameterPage("Channel 6", channel6);
}

void updatechannel7() {
  showCurrentParameterPage("Channel 7", channel7);
}

void updatechannel8() {
  showCurrentParameterPage("Channel 8", channel8);
}

void updatechannel9() {
  showCurrentParameterPage("Channel 9", channel9);
}

void updatechannel10() {
  showCurrentParameterPage("Channel 10", channel10);
}

void updatechannel11() {
  showCurrentParameterPage("Channel 11", channel11);
}

void updatechannel12() {
  showCurrentParameterPage("Channel 12", channel12);
}

void updatechannel13() {
  showCurrentParameterPage("Channel 13", channel13);
}

void updatechannel14() {
  showCurrentParameterPage("Channel 14", channel14);
}

void updatechannel15() {
  showCurrentParameterPage("Channel 15", channel15);
}

void updatechannel16() {
  showCurrentParameterPage("Channel 16", channel16);
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
    sr.writePin(GATE_NOTE1, LOW);
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
    sr.writePin(GATE_NOTE1, LOW);
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
  sr.writePin(GATE_NOTE1, LOW);  // All notes are off
}

void commandNote(int noteMsg) {
  unsigned int mV = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
  analogWrite(NOTE1, mV);
  sr.writePin(GATE_NOTE1, HIGH);
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
    sr.writePin(GATE_NOTE1, LOW);
    sr.writePin(GATE_NOTE2, LOW);
    sr.writePin(GATE_NOTE3, LOW);
    sr.writePin(GATE_NOTE4, LOW);
    sr.writePin(GATE_NOTE5, LOW);
    sr.writePin(GATE_NOTE6, LOW);
    sr.writePin(GATE_NOTE7, LOW);
    sr.writePin(GATE_NOTE8, LOW);
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
    sr.writePin(GATE_NOTE1, LOW);
    sr.writePin(GATE_NOTE2, LOW);
    sr.writePin(GATE_NOTE3, LOW);
    sr.writePin(GATE_NOTE4, LOW);
    sr.writePin(GATE_NOTE5, LOW);
    sr.writePin(GATE_NOTE6, LOW);
    sr.writePin(GATE_NOTE7, LOW);
    sr.writePin(GATE_NOTE8, LOW);
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
  sr.writePin(GATE_NOTE1, LOW);
  sr.writePin(GATE_NOTE2, LOW);
  sr.writePin(GATE_NOTE3, LOW);
  sr.writePin(GATE_NOTE4, LOW);
  sr.writePin(GATE_NOTE5, LOW);
  sr.writePin(GATE_NOTE6, LOW);
  sr.writePin(GATE_NOTE7, LOW);
  sr.writePin(GATE_NOTE8, LOW);
  // All notes are off
}

void commandNoteUni(int noteMsg) {

  unsigned int mV1 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
  analogWrite(NOTE1, mV1);
  unsigned int mV2 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
  analogWrite(NOTE2, mV2);
  unsigned int mV3 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
  analogWrite(NOTE3, mV3);
  unsigned int mV4 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
  analogWrite(NOTE4, mV4);
  unsigned int mV5 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
  analogWrite(NOTE5, mV5);
  unsigned int mV6 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[5] + 0.5);
  analogWrite(NOTE6, mV6);
  unsigned int mV7 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[6] + 0.5);
  analogWrite(NOTE7, mV7);
  unsigned int mV8 = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[7] + 0.5);
  analogWrite(NOTE8, mV8);

  sr.writePin(GATE_NOTE1, HIGH);
  sr.writePin(GATE_NOTE2, HIGH);
  sr.writePin(GATE_NOTE3, HIGH);
  sr.writePin(GATE_NOTE4, HIGH);
  sr.writePin(GATE_NOTE5, HIGH);
  sr.writePin(GATE_NOTE6, HIGH);
  sr.writePin(GATE_NOTE7, HIGH);
  sr.writePin(GATE_NOTE8, HIGH);
}

void myNoteOn(byte channel, byte note, byte velocity) {
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
        sr.writePin(GATE_NOTE1, HIGH);
        voiceOn[0] = true;
        break;

      case 2:
        voices[1].note = note;
        voices[1].velocity = velocity;
        voices[1].timeOn = millis();
        updateVoice2();
        sr.writePin(GATE_NOTE2, HIGH);
        voiceOn[1] = true;
        break;

      case 3:
        voices[2].note = note;
        voices[2].velocity = velocity;
        voices[2].timeOn = millis();
        updateVoice3();
        sr.writePin(GATE_NOTE3, HIGH);
        voiceOn[2] = true;
        break;

      case 4:
        voices[3].note = note;
        voices[3].velocity = velocity;
        voices[3].timeOn = millis();
        updateVoice4();
        sr.writePin(GATE_NOTE4, HIGH);
        voiceOn[3] = true;
        break;

      case 5:
        voices[4].note = note;
        voices[4].velocity = velocity;
        voices[4].timeOn = millis();
        updateVoice5();
        sr.writePin(GATE_NOTE5, HIGH);
        voiceOn[4] = true;
        break;

      case 6:
        voices[5].note = note;
        voices[5].velocity = velocity;
        voices[5].timeOn = millis();
        updateVoice6();
        sr.writePin(GATE_NOTE6, HIGH);
        voiceOn[5] = true;
        break;

      case 7:
        voices[6].note = note;
        voices[6].velocity = velocity;
        voices[6].timeOn = millis();
        updateVoice7();
        sr.writePin(GATE_NOTE7, HIGH);
        voiceOn[6] = true;
        break;

      case 8:
        voices[7].note = note;
        voices[7].velocity = velocity;
        voices[7].timeOn = millis();
        updateVoice8();
        sr.writePin(GATE_NOTE8, HIGH);
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
    analogWrite(VELOCITY1, velmV);
    analogWrite(VELOCITY2, velmV);
    analogWrite(VELOCITY3, velmV);
    analogWrite(VELOCITY4, velmV);
    analogWrite(VELOCITY5, velmV);
    analogWrite(VELOCITY6, velmV);
    analogWrite(VELOCITY7, velmV);
    analogWrite(VELOCITY8, velmV);
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

void myNoteOff(byte channel, byte note, byte velocity) {
  if (keyboardMode == 0) {
    switch (getVoiceNo(note)) {
      case 1:
        sr.writePin(GATE_NOTE1, LOW);
        voices[0].note = -1;
        voiceOn[0] = false;
        break;
      case 2:
        sr.writePin(GATE_NOTE2, LOW);
        voices[1].note = -1;
        voiceOn[1] = false;
        break;
      case 3:
        sr.writePin(GATE_NOTE3, LOW);
        voices[2].note = -1;
        voiceOn[2] = false;
        break;
      case 4:
        sr.writePin(GATE_NOTE4, LOW);
        voices[3].note = -1;
        voiceOn[3] = false;
        break;
      case 5:
        sr.writePin(GATE_NOTE5, LOW);
        voices[4].note = -1;
        voiceOn[4] = false;
        break;
      case 6:
        sr.writePin(GATE_NOTE6, LOW);
        voices[5].note = -1;
        voiceOn[5] = false;
        break;
      case 7:
        sr.writePin(GATE_NOTE7, LOW);
        voices[6].note = -1;
        voiceOn[6] = false;
        break;
      case 8:
        sr.writePin(GATE_NOTE8, LOW);
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
    analogWrite(VELOCITY1, velmV);
    analogWrite(VELOCITY2, velmV);
    analogWrite(VELOCITY3, velmV);
    analogWrite(VELOCITY4, velmV);
    analogWrite(VELOCITY5, velmV);
    analogWrite(VELOCITY6, velmV);
    analogWrite(VELOCITY7, velmV);
    analogWrite(VELOCITY8, velmV);
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
  sr.writePin(GATE_NOTE1, LOW);
  sr.writePin(GATE_NOTE2, LOW);
  sr.writePin(GATE_NOTE3, LOW);
  sr.writePin(GATE_NOTE4, LOW);
  sr.writePin(GATE_NOTE5, LOW);
  sr.writePin(GATE_NOTE6, LOW);
  sr.writePin(GATE_NOTE7, LOW);
  sr.writePin(GATE_NOTE8, LOW);

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
  gate1 = data[19].toInt();
  gate2 = data[20].toInt();
  gate3 = data[21].toInt();
  gate4 = data[22].toInt();
  gate5 = data[23].toInt();
  gate6 = data[24].toInt();
  gate7 = data[25].toInt();

  //MUX2

  //Switches


  //Patchname
  updatePatchname();
}

String getCurrentPatchData() {
  return patchName + "," + String(polycount) + "," + String(channel1) + "," + String(channel2) + "," + String(channel3) + "," + String(channel4)
         + "," + String(channel5) + "," + String(channel6) + "," + String(channel6) + "," + String(channel8)
         + "," + String(channel9) + "," + String(channel10) + "," + String(channel11) + "," + String(channel12)
         + "," + String(channel13) + "," + String(channel14) + "," + String(channel15) + "," + String(channel16)
         + "," + String(gate1) + "," + String(gate2) + "," + String(gate3) + "," + String(gate4)
         + "," + String(gate5) + "," + String(gate6) + "," + String(gate7) + "," + String(gate8);
}

void updatePatchname() {
  showPatchPage(String(patchNo), patchName);
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
    if (!paramEdit) {
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
        if (paramEdit) {
          channel1++;
          if (channel1 > CHANNEL_PARAMS) {
            channel1 = 0;
          }
        }
        updatechannel1();
        break;

      case 3:
        if (paramEdit) {
          channel2++;
          if (channel2 > CHANNEL_PARAMS) {
            channel2 = 0;
          }
        }
        updatechannel2();
        break;

      case 4:
        if (paramEdit) {
          channel3++;
          if (channel3 > CHANNEL_PARAMS) {
            channel3 = 0;
          }
        }
        updatechannel3();
        break;

      case 5:
        if (paramEdit) {
          channel4++;
          if (channel4 > CHANNEL_PARAMS) {
            channel4 = 0;
          }
        }
        updatechannel4();
        break;

      case 6:
        if (paramEdit) {
          channel5++;
          if (channel5 > CHANNEL_PARAMS) {
            channel5 = 0;
          }
        }
        updatechannel5();
        break;

      case 7:
        if (paramEdit) {
          channel6++;
          if (channel6 > CHANNEL_PARAMS) {
            channel6 = 0;
          }
        }
        updatechannel6();
        break;

      case 8:
        if (paramEdit) {
          channel7++;
          if (channel7 > CHANNEL_PARAMS) {
            channel7 = 0;
          }
        }
        updatechannel7();
        break;

      case 9:
        if (paramEdit) {
          channel8++;
          if (channel8 > CHANNEL_PARAMS) {
            channel8 = 0;
          }
        }
        updatechannel8();
        break;

      case 10:
        if (paramEdit) {
          channel9++;
          if (channel9 > CHANNEL_PARAMS) {
            channel9 = 0;
          }
        }
        updatechannel9();
        break;

      case 11:
        if (paramEdit) {
          channel10++;
          if (channel10 > CHANNEL_PARAMS) {
            channel10 = 0;
          }
        }
        updatechannel10();
        break;

      case 12:
        if (paramEdit) {
          channel11++;
          if (channel11 > CHANNEL_PARAMS) {
            channel11 = 0;
          }
        }
        updatechannel11();
        break;


      case 13:
        if (paramEdit) {
          channel12++;
          if (channel12 > CHANNEL_PARAMS) {
            channel12 = 0;
          }
        }
        updatechannel12();
        break;

      case 14:
        if (paramEdit) {
          channel13++;
          if (channel13 > CHANNEL_PARAMS) {
            channel13 = 0;
          }
        }
        updatechannel13();
        break;

      case 15:
        if (paramEdit) {
          channel14++;
          if (channel14 > CHANNEL_PARAMS) {
            channel14 = 0;
          }
        }
        updatechannel4();
        break;

      case 16:
        if (paramEdit) {
          channel15++;
          if (channel15 > CHANNEL_PARAMS) {
            channel15 = 0;
          }
        }
        updatechannel15();
        break;

      case 17:
        if (paramEdit) {
          channel16++;
          if (channel16 > CHANNEL_PARAMS) {
            channel16 = 0;
          }
        }
        updatechannel16();
        break;
    }

    param_encPrevious = param_encRead;

  } else if ((param_encCW && param_encRead < param_encPrevious - 3) || (!param_encCW && param_encRead > param_encPrevious + 3)) {
    if (!paramEdit) {
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
        if (paramEdit) {
          channel1--;
          if (channel1 < 0) {
            channel1 = CHANNEL_PARAMS;
          }
        }
        updatechannel1();
        break;

      case 3:
        if (paramEdit) {
          channel2--;
          if (channel2 < 0) {
            channel2 = CHANNEL_PARAMS;
          }
        }
        updatechannel2();
        break;

      case 4:
        if (paramEdit) {
          channel3--;
          if (channel3 < 0) {
            channel3 = CHANNEL_PARAMS;
          }
        }
        updatechannel3();
        break;

      case 5:
        if (paramEdit) {
          channel4--;
          if (channel4 < 0) {
            channel4 = CHANNEL_PARAMS;
          }
        }
        updatechannel4();
        break;

      case 6:
        if (paramEdit) {
          channel5--;
          if (channel5 < 0) {
            channel5 = CHANNEL_PARAMS;
          }
        }
        updatechannel5();
        break;

      case 7:
        if (paramEdit) {
          channel6--;
          if (channel6 < 0) {
            channel6 = CHANNEL_PARAMS;
          }
        }
        updatechannel6();
        break;

      case 8:
        if (paramEdit) {
          channel7--;
          if (channel7 < 0) {
            channel7 = CHANNEL_PARAMS;
          }
        }
        updatechannel7();
        break;

      case 9:
        if (paramEdit) {
          channel8--;
          if (channel8 < 0) {
            channel8 = CHANNEL_PARAMS;
          }
        }
        updatechannel8();
        break;

      case 10:
        if (paramEdit) {
          channel9--;
          if (channel9 < 0) {
            channel9 = CHANNEL_PARAMS;
          }
        }
        updatechannel9();
        break;

      case 11:
        if (paramEdit) {
          channel10--;
          if (channel10 < 0) {
            channel10 = CHANNEL_PARAMS;
          }
        }
        updatechannel10();
        break;

      case 12:
        if (paramEdit) {
          channel11--;
          if (channel11 < 0) {
            channel11 = CHANNEL_PARAMS;
          }
        }
        updatechannel11();
        break;

      case 13:
        if (paramEdit) {
          channel12--;
          if (channel12 < 0) {
            channel12 = CHANNEL_PARAMS;
          }
        }
        updatechannel12();
        break;

      case 14:
        if (paramEdit) {
          channel13--;
          if (channel13 < 0) {
            channel13 = CHANNEL_PARAMS;
          }
        }
        updatechannel13();
        break;

      case 15:
        if (paramEdit) {
          channel14--;
          if (channel14 < 0) {
            channel14 = CHANNEL_PARAMS;
          }
        }
        updatechannel14();
        break;

      case 16:
        if (paramEdit) {
          channel15--;
          if (channel15 < 0) {
            channel15 = CHANNEL_PARAMS;
          }
        }
        updatechannel15();
        break;

      case 17:
        if (paramEdit) {
          channel16--;
          if (channel16 < 0) {
            channel16 = CHANNEL_PARAMS;
          }
        }
        updatechannel16();
        break;
    }
    param_encPrevious = param_encRead;
  }
}

void checkSwitches() {

  paramButton.update();
  if (paramButton.numClicks() == 1) {
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
  midi1.read(midiChannel);    //USB HOST MIDI Class Compliant
  MIDI.read(midiChannel);     //MIDI 5 Pin DIN
  usbMIDI.read(midiChannel);  //USB Client MIDI
  sr.update();
}

// void updateMenu() {  // Called whenever button is pushed


//       case SCALE_FACTOR:
//         setCh = mod(encoderPos, 7);
//         switch (setCh) {
//           case 0:
//           case 1:
//           case 2:
//           case 3:
//           case 4:
//           case 5:
//             menu = SCALE_FACTOR_SET_CH;
//             break;
//           case 6:
//             menu = SETTINGS;
//             break;
//         }
//         break;


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
