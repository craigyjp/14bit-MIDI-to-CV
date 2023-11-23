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
#include <ShiftRegister74HC595.h>


// OLED I2C is used on pins 18 and 19 for Teensy 3.x

// Voices available
#define NO_OF_VOICES 8
#define trigTimeout 20

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

#include "ST7735Display.h"

boolean cardStatus = false;

char gateTrig[] = "TTTTTTTT";


float sfAdj[8];
float noteTrig[8];
float monoTrig;
float unisonTrig;

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
unsigned long trigTimer = 0;

// MIDI setup

//USB HOST MIDI Class Compliant
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
const int channel = 1;

int patchNo = 1;  //Current patch no

// create a global shift register object
// parameters: <number of shift registers> (data pin, clock pin, latch pin)
ShiftRegister74HC595<4> sr(30, 31, 32);

void setup() {

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
    reinitialiseToPanel();
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
}

void commandNote(int noteMsg) {
  unsigned int mV = (unsigned int)((float)(noteMsg + transpose + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
  analogWrite(NOTE1, mV);
  sr.set(GATE_NOTE1, HIGH);
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
    sr.set(GATE_NOTE1, LOW);
    sr.set(GATE_NOTE2, LOW);
    sr.set(GATE_NOTE3, LOW);
    sr.set(GATE_NOTE4, LOW);
    sr.set(GATE_NOTE5, LOW);
    sr.set(GATE_NOTE6, LOW);
    sr.set(GATE_NOTE7, LOW);
    sr.set(GATE_NOTE8, LOW);
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
    sr.set(GATE_NOTE1, LOW);
    sr.set(GATE_NOTE2, LOW);
    sr.set(GATE_NOTE3, LOW);
    sr.set(GATE_NOTE4, LOW);
    sr.set(GATE_NOTE5, LOW);
    sr.set(GATE_NOTE6, LOW);
    sr.set(GATE_NOTE7, LOW);
    sr.set(GATE_NOTE8, LOW);
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
  sr.set(GATE_NOTE1, LOW);
  sr.set(GATE_NOTE2, LOW);
  sr.set(GATE_NOTE3, LOW);
  sr.set(GATE_NOTE4, LOW);
  sr.set(GATE_NOTE5, LOW);
  sr.set(GATE_NOTE6, LOW);
  sr.set(GATE_NOTE7, LOW);
  sr.set(GATE_NOTE8, LOW);
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

  sr.set(GATE_NOTE1, HIGH);
  sr.set(GATE_NOTE2, HIGH);
  sr.set(GATE_NOTE3, HIGH);
  sr.set(GATE_NOTE4, HIGH);
  sr.set(GATE_NOTE5, HIGH);
  sr.set(GATE_NOTE6, HIGH);
  sr.set(GATE_NOTE7, HIGH);
  sr.set(GATE_NOTE8, HIGH);
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
        sr.set(GATE_NOTE1, HIGH);
        voiceOn[0] = true;
        break;

      case 2:
        voices[1].note = note;
        voices[1].velocity = velocity;
        voices[1].timeOn = millis();
        updateVoice2();
        sr.set(GATE_NOTE2, HIGH);
        voiceOn[1] = true;
        break;

      case 3:
        voices[2].note = note;
        voices[2].velocity = velocity;
        voices[2].timeOn = millis();
        updateVoice3();
        sr.set(GATE_NOTE3, HIGH);
        voiceOn[2] = true;
        break;

      case 4:
        voices[3].note = note;
        voices[3].velocity = velocity;
        voices[3].timeOn = millis();
        updateVoice4();
        sr.set(GATE_NOTE4, HIGH);
        voiceOn[3] = true;
        break;

      case 5:
        voices[4].note = note;
        voices[4].velocity = velocity;
        voices[4].timeOn = millis();
        updateVoice5();
        sr.set(GATE_NOTE5, HIGH);
        voiceOn[4] = true;
        break;

      case 6:
        voices[5].note = note;
        voices[5].velocity = velocity;
        voices[5].timeOn = millis();
        updateVoice6();
        sr.set(GATE_NOTE6, HIGH);
        voiceOn[5] = true;
        break;

      case 7:
        voices[6].note = note;
        voices[6].velocity = velocity;
        voices[6].timeOn = millis();
        updateVoice7();
        sr.set(GATE_NOTE7, HIGH);
        voiceOn[6] = true;
        break;

      case 8:
        voices[7].note = note;
        voices[7].velocity = velocity;
        voices[7].timeOn = millis();
        updateVoice8();
        sr.set(GATE_NOTE8, HIGH);
        voiceOn[7] = true;
        break;
    }
  } else if (keyboardMode == 4 || keyboardMode == 5 || keyboardMode == 6) {
    if (keyboardMode == 4) {
      S1 = 1;
      S2 = 1;
    }
    if (keyboardMode == 5) {
      S1 = 0;
      S2 = 1;
    }
    if (keyboardMode == 6) {
      S1 = 0;
      S2 = 0;
    }
    noteMsg = note;

    if (velocity == 0) {
      notes[noteMsg] = false;
    } else {
      notes[noteMsg] = true;
    }

    unsigned int velmV = map(velocity, 0, 127, 0, 8191);
    analogWrite(VELOCITY1, velmV);
    if (S1 && S2) {  // Highest note priority
      commandTopNote();
    } else if (!S1 && S2) {  // Lowest note priority
      commandBottomNote();
    } else {                 // Last note priority
      if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
        orderIndx = (orderIndx + 1) % 40;
        noteOrder[orderIndx] = noteMsg;
      }
      commandLastNote();
    }
  } else if (keyboardMode == 1 || keyboardMode == 2 || keyboardMode == 3) {
    if (keyboardMode == 1) {
      S1 = 1;
      S2 = 1;
    }
    if (keyboardMode == 2) {
      S1 = 0;
      S2 = 1;
    }
    if (keyboardMode == 3) {
      S1 = 0;
      S2 = 0;
    }
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
    if (S1 && S2) {  // Highest note priority
      commandTopNoteUni();
    } else if (!S1 && S2) {  // Lowest note priority
      commandBottomNoteUni();
    } else {                 // Last note priority
      if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
        orderIndx = (orderIndx + 1) % 40;
        noteOrder[orderIndx] = noteMsg;
      }
      commandLastNoteUni();
    }
  }
}

void myNoteOff(byte channel, byte note, byte velocity) {
  if (keyboardMode == 0) {
    switch (getVoiceNo(note)) {
      case 1:
        sr.set(GATE_NOTE1, LOW);
        voices[0].note = -1;
        voiceOn[0] = false;
        break;
      case 2:
        sr.set(GATE_NOTE2, LOW);
        voices[1].note = -1;
        voiceOn[1] = false;
        break;
      case 3:
        sr.set(GATE_NOTE3, LOW);
        voices[2].note = -1;
        voiceOn[2] = false;
        break;
      case 4:
        sr.set(GATE_NOTE4, LOW);
        voices[3].note = -1;
        voiceOn[3] = false;
        break;
      case 5:
        sr.set(GATE_NOTE5, LOW);
        voices[4].note = -1;
        voiceOn[4] = false;
        break;
      case 6:
        sr.set(GATE_NOTE6, LOW);
        voices[5].note = -1;
        voiceOn[5] = false;
        break;
      case 7:
        sr.set(GATE_NOTE7, LOW);
        voices[6].note = -1;
        voiceOn[6] = false;
        break;
      case 8:
        sr.set(GATE_NOTE8, LOW);
        voices[7].note = -1;
        voiceOn[7] = false;
        break;
    }
  } else if (keyboardMode == 4 || keyboardMode == 5 || keyboardMode == 6) {
    if (keyboardMode == 4) {
      S1 = 1;
      S2 = 1;
    }
    if (keyboardMode == 5) {
      S1 = 0;
      S2 = 1;
    }
    if (keyboardMode == 6) {
      S1 = 0;
      S2 = 0;
    }
    noteMsg = note;

    if (velocity == 0) {
      notes[noteMsg] = false;
    } else {
      notes[noteMsg] = true;
    }

    // Pins NP_SEL1 and NP_SEL2 indictate note priority

    unsigned int velmV = map(velocity, 0, 127, 0, 8191);
    analogWrite(VELOCITY1, velmV);
    if (S1 && S2) {  // Highest note priority
      commandTopNote();
    } else if (!S1 && S2) {  // Lowest note priority
      commandBottomNote();
    } else {                 // Last note priority
      if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
        orderIndx = (orderIndx + 1) % 40;
        noteOrder[orderIndx] = noteMsg;
      }
      commandLastNote();
    }
  } else if (keyboardMode == 1 || keyboardMode == 2 || keyboardMode == 3) {
    if (keyboardMode == 1) {
      S1 = 1;
      S2 = 1;
    }
    if (keyboardMode == 2) {
      S1 = 0;
      S2 = 1;
    }
    if (keyboardMode == 3) {
      S1 = 0;
      S2 = 0;
    }
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
    if (S1 && S2) {  // Highest note priority
      commandTopNoteUni();
    } else if (!S1 && S2) {  // Lowest note priority
      commandBottomNoteUni();
    } else {                 // Last note priority
      if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
        orderIndx = (orderIndx + 1) % 40;
        noteOrder[orderIndx] = noteMsg;
      }
      commandLastNoteUni();
    }
  }
}

int getVoiceNo(int note) {
  voiceToReturn = -1;       //Initialise to 'null'
  earliestTime = millis();  //Initialise to now
  if (note == -1) {
    //NoteOn() - Get the oldest free voice (recent voices may be still on release stage)
    for (int i = 0; i < NO_OF_VOICES; i++) {
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
      for (int i = 0; i < NO_OF_VOICES; i++) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    return voiceToReturn + 1;
  } else {
    //NoteOff() - Get voice number from note
    for (int i = 0; i < NO_OF_VOICES; i++) {
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
  testparam = data[1].toInt();

  //MUX2

  //Switches


  //Patchname
  updatePatchname();

  //  Serial.print("Set Patch: ");
  //  Serial.println(patchName);
}

String getCurrentPatchData() {
  return patchName + "," + String(testparam);
}

void updatePatchname() {
  showPatchPage(String(patchNo), patchName);
}

void reinitialiseToPanel() {
  //This sets the current patch to be the same as the current hardware panel state - all the pots
  //The four button controls stay the same state
  //This reinialises the previous hardware values to force a re-read
  patchName = INITPATCHNAME;
  showPatchPage("Initial", "Panel Settings");
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

void checkSwitches() {

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
  if (settingsButton.held()) {
    //If recall held, set current patch to match current hardware state
    //Reinitialise all hardware values to force them to be re-read if different
    state = REINITIALISE;
    reinitialiseToPanel();
  } else if (settingsButton.numClicks() == 1) {
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
  checkeepromChanges();
  checkEncoder();
  myusb.Task();
  midi1.read(midiChannel);    //USB HOST MIDI Class Compliant
  MIDI.read(midiChannel);     //MIDI 5 Pin DIN
  usbMIDI.read(midiChannel);  //USB Client MIDI
  
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
