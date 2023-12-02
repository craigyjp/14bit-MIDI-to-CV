#include "SettingsService.h"

void settingsMIDICh(int index, const char *value);
void settingsGATECh(int index, const char *value);
void settingsKeyMode(int index, const char *value);
void settingsTranspose(int index, const char *value);
void settingsModWheelDepth(int index, const char *value);
void settingsEncoderDir(char *value);
void settingsSFAdj1(char *value);
void settingsSFAdj2(char *value);
void settingsSFAdj3(char *value);
void settingsSFAdj4(char *value);
void settingsSFAdj5(char *value);
void settingsSFAdj6(char *value);
void settingsSFAdj7(char *value);
void settingsSFAdj8(char *value);

int currentIndexMIDICh();
int currentIndexGATECh();
int currentKeyMode();
int currentIndexTranspose();
int currentIndexModWheelDepth();
int currentIndexEncoderDir();
int currentIndexSFAdj1();
int currentIndexSFAdj2();
int currentIndexSFAdj3();
int currentIndexSFAdj4();
int currentIndexSFAdj5();
int currentIndexSFAdj6();
int currentIndexSFAdj7();
int currentIndexSFAdj8();

void settingsMIDICh(int index, const char *value) {
  if (strcmp(value, "ALL") == 0) {
    midiChannel = MIDI_CHANNEL_OMNI;
  } else {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

void settingsGATECh(int index, const char *value) {
    if (strcmp(value, "ALL") == 0) {
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

void settingsSFAdj1(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeSFAdjust(0, 1);
}

void settingsSFAdj2(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeSFAdjust(1, 1);
}

void settingsSFAdj3(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeSFAdjust(2, 1);
}

void settingsSFAdj4(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeSFAdjust(3, 1);
}

void settingsSFAdj5(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeSFAdjust(4, 1);
}

void settingsSFAdj6(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeSFAdjust(5, 1);
}

void settingsSFAdj7(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeSFAdjust(6, 1);
}

void settingsSFAdj8(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeSFAdjust(7, 1);
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

int currentIndexSFAdj1() {
  return getSFAdjust(0);
}

int currentIndexSFAdj2() {
  return getSFAdjust(1);
}

int currentIndexSFAdj3() {
  return getSFAdjust(2);
}

int currentIndexSFAdj4() {
  return getSFAdjust(3);
}

int currentIndexSFAdj5() {
  return getSFAdjust(4);
}

int currentIndexSFAdj6() {
  return getSFAdjust(5);
}

int currentIndexSFAdj7() {
  return getSFAdjust(6);
}

int currentIndexSFAdj8() {
  return getSFAdjust(7);
}

// add settings to the circular buffer
void setUpSettings() {
  settings::append(settings::SettingsOption{ "MIDI Ch.", { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" }, settingsMIDICh, currentIndexMIDICh });
  settings::append(settings::SettingsOption{ "Gate Ch.", { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" }, settingsGATECh, currentIndexGATECh });
  settings::append(settings::SettingsOption{ "Key Mode", { "Poly", "Unison T", "Unison B", "Unison L", "Mono T", "Mono B", "Mono L", "\0" }, settingsKeyMode, currentIndexKeyMode });
  settings::append(settings::SettingsOption{ "Transpose", { "-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "+11", "+12", "\0" }, settingsTranspose, currentIndexTranspose });
  settings::append(settings::SettingsOption{ "Octave", { "-2", "-1", "0", "+1", "+2", "\0" }, settingsOctave, currentIndexOctave });
  settings::append(settings::SettingsOption{ "Encoder", { "Type 1", "Type 2", "\0" }, settingsEncoderDir, currentIndexEncoderDir });
  settings::append(settings::SettingsOption{ "SF Adjust 1", { "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "\0" }, settingsSFAdj1, currentIndexSFAdj1 });
  settings::append(settings::SettingsOption{ "SF Adjust 2", { "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "\0" }, settingsSFAdj2, currentIndexSFAdj2 });
  settings::append(settings::SettingsOption{ "SF Adjust 3", { "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "\0" }, settingsSFAdj3, currentIndexSFAdj3 });
  settings::append(settings::SettingsOption{ "SF Adjust 4", { "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "\0" }, settingsSFAdj4, currentIndexSFAdj4 });
  settings::append(settings::SettingsOption{ "SF Adjust 5", { "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "\0" }, settingsSFAdj5, currentIndexSFAdj5 });
  settings::append(settings::SettingsOption{ "SF Adjust 6", { "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "\0" }, settingsSFAdj6, currentIndexSFAdj6 });
  settings::append(settings::SettingsOption{ "SF Adjust 7", { "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "\0" }, settingsSFAdj7, currentIndexSFAdj7 });
  settings::append(settings::SettingsOption{ "SF Adjust 8", { "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "\0" }, settingsSFAdj8, currentIndexSFAdj8 });

}
