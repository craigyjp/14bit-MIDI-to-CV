#include "SettingsService.h"

void settingsMIDICh(int index, const char *value);
void settingsGATECh(int index, const char *value);
void settingsKeyMode(int index, const char *value);
void settingsTranspose(int index, const char *value);
void settingsModWheelDepth(int index, const char *value);
void settingsEncoderDir(char *value);

int currentIndexMIDICh();
int currentIndexGATECh();
int currentKeyMode();
int currentIndexTranspose();
int currentIndexModWheelDepth();
int currentIndexEncoderDir();

void settingsMIDICh(int index, const char *value) {
  if (strcmp(value, "All") == 0) {
    midiChannel = MIDI_CHANNEL_OMNI;
  } else {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

void settingsGATECh(int index, const char *value) {
  if (strcmp(value, "All") == 0) {
    gateChannel = MIDI_CHANNEL_OMNI;
  } else {
    gateChannel = atoi(value);
  }
  storeGATEChannel(gateChannel);
}

void settingsKeyMode(int index, const char *value) {
  if (strcmp(value, "Poly") == 0) {
    keyboardMode = 0;
  }
  if (strcmp(value, "Unison T") == 0) {
    keyboardMode = 1;
  }
  if (strcmp(value, "Unison B") == 0) {
    keyboardMode = 2;
  }
  if (strcmp(value, "Unison L") == 0) {
    keyboardMode = 3;
  }
  if (strcmp(value, "Mono T") == 0) {
    keyboardMode = 4;
  }
  if (strcmp(value, "Mono B") == 0) {
    keyboardMode = 5;
  }
  if (strcmp(value, "Mono L") == 0) {
    keyboardMode = 6;
  }
  storeKeyMode(keyboardMode);
}

void settingsTranspose(int index, const char *value) {
  if (strcmp(value, "-12") == 0) {
    eepromtranspose = 0;
  }
  if (strcmp(value, "-11") == 0) {
    eepromtranspose = 1;
  }
  if (strcmp(value, "-10") == 0) {
    eepromtranspose = 2;
  }
  if (strcmp(value, "-9") == 0) {
    eepromtranspose = 3;
  }
  if (strcmp(value, "-8") == 0) {
    eepromtranspose = 4;
  }
  if (strcmp(value, "-7") == 0) {
    eepromtranspose = 5;
  }
  if (strcmp(value, "-6") == 0) {
    eepromtranspose = 6;
  }
  if (strcmp(value, "-5") == 0) {
    eepromtranspose = 7;
  }
  if (strcmp(value, "-4") == 0) {
    eepromtranspose = 8;
  }
  if (strcmp(value, "-3") == 0) {
    eepromtranspose = 9;
  }
  if (strcmp(value, "-2") == 0) {
    eepromtranspose = 10;
  }
  if (strcmp(value, "-1") == 0) {
    eepromtranspose = 11;
  }
  if (strcmp(value, "0") == 0) {
    eepromtranspose = 12;
  }
  if (strcmp(value, "+1") == 0) {
    eepromtranspose = 13;
  }
  if (strcmp(value, "+2") == 0) {
    eepromtranspose = 14;
  }
  if (strcmp(value, "+3") == 0) {
    eepromtranspose = 15;
  }
  if (strcmp(value, "+4") == 0) {
    eepromtranspose = 16;
  }
  if (strcmp(value, "+5") == 0) {
    eepromtranspose = 17;
  }
  if (strcmp(value, "+6") == 0) {
    eepromtranspose = 18;
  }
  if (strcmp(value, "+7") == 0) {
    eepromtranspose = 19;
  }
  if (strcmp(value, "+8") == 0) {
    eepromtranspose = 20;
  }
  if (strcmp(value, "+9") == 0) {
    eepromtranspose = 21;
  }
  if (strcmp(value, "+10") == 0) {
    eepromtranspose = 22;
  }
  if (strcmp(value, "+11") == 0) {
    eepromtranspose = 23;
  }
  if (strcmp(value, "+12") == 0) {
    eepromtranspose = 24;
  }
  storeTranspose(eepromtranspose);
}

void settingsOctave(int index, const char *value) {
  if (strcmp(value, "-2") == 0) {
    eepromOctave = 0;
  }
  if (strcmp(value, "-1") == 0) {
    eepromOctave = 1;
  }
  if (strcmp(value, "0") == 0) {
    eepromOctave = 2;
  }
  if (strcmp(value, "+1") == 0) {
    eepromOctave = 3;
  }
  if (strcmp(value, "+2") == 0) {
    eepromOctave = 4;
  }
  storeOctave(eepromOctave);
}

void settingsEncoderDir(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeEncoderDir(encCW ? 1 : 0);
}

int currentIndexMIDICh() {
  return getMIDIChannel();
}

int currentIndexGATECh() {
  return getGATEChannel();
}

int currentIndexKeyMode() {
  return getKeyMode();
}

int currentIndexTranspose() {
  return getTranspose();
}

int currentIndexOctave() {
  return getOctave();
}

int currentIndexEncoderDir() {
  return getEncoderDir() ? 0 : 1;
}

// add settings to the circular buffer
void setUpSettings() {
  settings::append(settings::SettingsOption{ "MIDI Ch.", { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" }, settingsMIDICh, currentIndexMIDICh });
  settings::append(settings::SettingsOption{ "Gate Ch.", { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" }, settingsGATECh, currentIndexGATECh });
  settings::append(settings::SettingsOption{ "Key Mode", { "Poly", "Unison T", "Unison B", "Unison L", "Mono T", "Mono B", "Mono L", "\0" }, settingsKeyMode, currentIndexKeyMode });
  settings::append(settings::SettingsOption{ "Transpose", { "-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "+11", "+12", "\0" }, settingsTranspose, currentIndexTranspose });
  settings::append(settings::SettingsOption{ "Octave", { "-2", "-1", "0", "+1", "+2", "\0" }, settingsOctave, currentIndexOctave });
  settings::append(settings::SettingsOption{ "Encoder", { "Type 1", "Type 2", "\0" }, settingsEncoderDir, currentIndexEncoderDir });
}
