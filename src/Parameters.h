// Parameters

uint8_t GATE_PINS[8] = {
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
};

uint8_t GATE_NOTES[8] = {
};

Gate GATE1(0);
Gate GATE2(1);
Gate GATE3(2);
Gate GATE4(3);
Gate GATE5(4);
Gate GATE6(5);
Gate GATE7(6);
Gate GATE8(7);

Gate *GATES[] = {&GATE1, &GATE2, &GATE3, &GATE4, &GATE5, &GATE6, &GATE7, &GATE8};
int LED_IS_ON = 0;

float sfAdj[8];

unsigned long timeout = 0;

String patchName = INITPATCHNAME;
boolean encCW = true;//This is to set the encoder to increment when turned CW - Settings Option
boolean param_encCW = true;
int param_number = 0;
boolean paramEdit = false;
boolean paramChange = false;

unsigned int mV1;
unsigned int mV2;
unsigned int mV3;
unsigned int mV4;
unsigned int mV5;
unsigned int mV6;
unsigned int mV7;
unsigned int mV8;

//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = 1;//(EEPROM)
byte gateChannel = 2;//(EEPROM)
int freeGates = 0;

int polycount = 0;
int channel1 = 0;
int channel2 = 0;
int channel3 = 0;
int channel4 = 0;
int channel5 = 0;
int channel6 = 0;
int channel7 = 0;
int channel8 = 0;
int channel9 = 0;
int channel10 = 0;
int channel11 = 0;
int channel12 = 0;
int channel13 = 0;
int channel14 = 0;
int channel15 = 0;
int channel16 = 0;

int gate1 = 36;
int gate2 = 36;
int gate3 = 36;
int gate4 = 36;
int gate5 = 36;
int gate6 = 35;
int gate7 = 36;
int gate8 = 36;


int transpose;
int eepromtranspose;
int oldeepromtranspose;
int8_t i;
int noteMsg;
int octave;
int eepromOctave;
int oldeepromOctave;
int realoctave;
int keyboardMode = 0;

int returnvalue = 0;
