#include <EEPROM.h>

#define EEPROM_MIDI_CH 0
#define EEPROM_ENCODER_DIR 1
#define EEPROM_CLOCKSOURCE 2
#define EEPROM_PITCHBEND 3
#define EEPROM_MODWHEEL_DEPTH 4
#define EEPROM_LAST_PATCH 5
#define EEPROM_GATE_CH 6

// EEPROM Addresses

#define ADDR_TRANSPOSE 26
#define ADDR_OCTAVE 28
#define ADDR_KEYBOARD_MODE 30
#define ADDR_SF_ADJUST 32


int getMIDIChannel() {
  byte midiChannel = EEPROM.read(EEPROM_MIDI_CH);
  if (midiChannel < 0 || midiChannel > 16) midiChannel = MIDI_CHANNEL_OMNI;//If EEPROM has no MIDI channel stored
  return midiChannel;
}

void storeMidiChannel(byte channel)
{
  EEPROM.update(EEPROM_MIDI_CH, channel);
}

int getGATEChannel() {
  byte gateChannel = EEPROM.read(EEPROM_GATE_CH);
  if (gateChannel < 0 || gateChannel > 16) gateChannel = MIDI_CHANNEL_OMNI;//If EEPROM has no MIDI channel stored
  return gateChannel;
}

void storeGATEChannel(byte channel)
{
  EEPROM.update(EEPROM_GATE_CH, channel);
}

int getKeyMode() {
  byte keyboardMode = EEPROM.read(ADDR_KEYBOARD_MODE);
  if (keyboardMode < 0 || keyboardMode > 6) keyboardMode = 0;
  return keyboardMode;
}

void storeKeyMode(byte keyboardMode)
{
  EEPROM.update(ADDR_KEYBOARD_MODE, keyboardMode);
}

boolean getEncoderDir() {
  byte ed = EEPROM.read(EEPROM_ENCODER_DIR); 
  if (ed < 0 || ed > 1)return true; //If EEPROM has no encoder direction stored
  return ed == 1 ? true : false;
}

void storeEncoderDir(byte encoderDir)
{
  EEPROM.update(EEPROM_ENCODER_DIR, encoderDir);
}

int getTranspose() {
  byte eepromtranspose = EEPROM.read(ADDR_TRANSPOSE);
  if (eepromtranspose < 0 || eepromtranspose > 25) eepromtranspose = 13;
  return eepromtranspose;
}

void storeTranspose(byte eepromtranspose)
{
  EEPROM.update(ADDR_TRANSPOSE, eepromtranspose);
}

int getSFAdjust(byte SFAdjustNumber) {
  sfAdj[SFAdjustNumber] = EEPROM.read(ADDR_SF_ADJUST + SFAdjustNumber);
  if (sfAdj[SFAdjustNumber] < 0 || sfAdj[SFAdjustNumber] > 25) sfAdj[SFAdjustNumber] = 13;
  return sfAdj[SFAdjustNumber];
}

void storeSFAdjust(byte SFAdjustNumber, byte SFAdjustValue)
{
  EEPROM.update(ADDR_SF_ADJUST + SFAdjustNumber, sfAdj[SFAdjustNumber]);
}

int getOctave() {
  byte eepromOctave = EEPROM.read(ADDR_OCTAVE);
  if (eepromOctave < 0 || eepromOctave > 4) eepromOctave = 2; //If EEPROM has no mod wheel depth stored
  return eepromOctave;
}

void storeOctave(byte eepromOctave)
{
  EEPROM.update(ADDR_OCTAVE, eepromOctave);
}

int getLastPatch() {
  int lastPatchNumber = EEPROM.read(EEPROM_LAST_PATCH);
  if (lastPatchNumber < 1 || lastPatchNumber > 999) lastPatchNumber = 1;
  return lastPatchNumber;
}

void storeLastPatch(int lastPatchNumber)
{
  EEPROM.update(EEPROM_LAST_PATCH, lastPatchNumber);
}

