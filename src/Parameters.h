// Parameters

static unsigned long clock_timer = 0, clock_timeout = 0;
static unsigned int clock_count = 0;

int Patchnumber = 0;
unsigned long timeout = 0;
unsigned long firsttimer = 0;

String patchName = INITPATCHNAME;
boolean encCW = true;//This is to set the encoder to increment when turned CW - Settings Option

//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = 1;//(EEPROM)
boolean announce = false;

int testparam = 0;

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
