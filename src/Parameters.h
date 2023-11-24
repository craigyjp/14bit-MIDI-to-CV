// Parameters

// static unsigned long clock_timer = 0, clock_timeout = 0;
// static unsigned int clock_count = 0;

unsigned long timeout = 0;

String patchName = INITPATCHNAME;
boolean encCW = true;//This is to set the encoder to increment when turned CW - Settings Option
boolean param_encCW = true;
int param_number = 0;
boolean paramEdit = false;
boolean forwards = false;

//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = 1;//(EEPROM)

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

int gate1 = 0;
int gate2 = 0;
int gate3 = 0;
int gate4 = 0;
int gate5 = 0;
int gate6 = 0;
int gate7 = 0;
int gate8 = 0;


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
