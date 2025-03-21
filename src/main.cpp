#include <Arduino.h>
#include <arduinoFFT.h>
#include <Wire.h>
#include "EEPROM.h"
#include <SI4735.h>
#include "avdweb_Switch.h"
#include <ESP32RotaryEncoder.h>
#include "global.h"
#include "ui.h"
#include "patch_full.h"          // SSB patch for whole SSBRX initialization string


UI ui;
Switch pushButton = Switch(ROTARY_ENCODER_BUTTON_PIN);
RotaryEncoder encoder = RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, RE_DEFAULT_PIN, ROTARY_ENCODER_VCC_PIN);
SI4735 rx;

// SI473/5 and UI
#define MIN_ELAPSED_TIME         5  // 300
#define MIN_ELAPSED_RSSI_TIME  200  // RSSI check uses IN_ELAPSED_RSSI_TIME * 6 = 1.2s
#define ELAPSED_COMMAND      10000  // time to turn off the last command controlled by encoder. Time to goes back to the VFO control // G8PTN: Increased time and corrected comment
#define ELAPSED_CLICK         1500  // time to check the double click commands
#define DEFAULT_VOLUME          35  // change it for your favorite sound volume
#define RDS_CHECK_TIME         250  // Increased from 90

#define BACKGROUND_REFRESH_TIME 100    // Background screen refresh time. Covers the situation where there are no other events causing a refresh

// SI4732/5 patch
const uint16_t size_content = sizeof ssb_patch_content; // see patch_init.h

#define EEPROM_SIZE     512
#define STORE_TIME    10000                  // Time of inactivity to make the current receiver status writable (10s)

// EEPROM 
// ====================================================================================================================================================
// Update F/W version comment as required   F/W VER    Function                                                           Locn (dec)            Bytes
// ====================================================================================================================================================
const uint8_t  app_id = 65;          //               EEPROM ID.  If EEPROM read value mismatch, reset EEPROM            eeprom_address        1
const uint16_t app_ver = 101;         //     v1.01     EEPROM VER. If EEPROM read value mismatch (older), reset EEPROM    eeprom_ver_address    2
const int eeprom_address = 0;         //               EEPROM start address
const int eeprom_set_address = 256;   //               EEPROM setting base adddress
const int eeprom_setp_address = 272;  //               EEPROM setting (per band) base adddress
const int eeprom_ver_address = 496;   //               EEPROM version base adddress

long storeTime = millis();
bool itIsTimeToSave = false;

bool infoShow = false;
char infoMessage[40];

bool bfoOn = false;
bool ssbLoaded = false;

bool muted = false;
int8_t agcIdx = 0;
uint8_t disableAgc = 0;
int8_t agcNdx = 0;
int8_t softMuteMaxAttIdx = 4;
uint8_t countClick = 0;

uint8_t seekDirection = 1;
bool seekStop = false;        // G8PTN: Added flag to abort seeking on rotary encoder detection

bool cmdBand = false;
bool cmdVolume = false;
bool cmdAgc = false;
bool cmdBandwidth = false;
bool cmdStep = false;
bool cmdMode = false;
bool cmdMenu = false;
bool cmdSoftMuteMaxAtt = false;
bool cmdCal = false;
bool cmdAvc = false;

bool fmRDS = false;

int16_t currentBFO = 0;
long elapsedRSSI = millis();
long elapsedButton = millis();

long lastStrengthCheck = millis();
long lastRDSCheck = millis();

long elapsedClick = millis();
long elapsedCommand = millis();
volatile int encoderCount = 0;
uint16_t currentFrequency;

const uint16_t currentBFOStep = 10;

// G8PTN: Main additional variables
// BFO and Calibration limits (BFOMax + CALMax <= 16000)
const int BFOMax = 14000;               // Maximum range for currentBFO = +/- BFOMax
const int CALMax = 2000;               // Maximum range for currentCAL = +/- CALMax

// AGC/ATTN index per mode (FM/AM/SSB)
int8_t FmAgcIdx = 0;                    // Default FM  AGGON  : Range = 0 to 37, 0 = AGCON, 1 - 27 = ATTN 0 to 26 
int8_t AmAgcIdx = 0;                    // Default AM  AGCON  : Range = 0 to 37, 0 = AGCON, 1 - 37 = ATTN 0 to 36
int8_t SsbAgcIdx = 0;                   // Default SSB AGCON  : Range = 0 to 1,  0 = AGCON,      1 = ATTN 0

// AVC index per mode (AM/SSB)
int8_t AmAvcIdx = 48;                   // Default AM  = 48 (as per AN332), range = 12 to 90 in steps of 2
int8_t SsbAvcIdx = 48;                  // Default SSB = 48, range = 12 to 90 in steps of 2

// SoftMute index per mode (AM/SSB)
int8_t AmSoftMuteIdx = 4;               // Default AM  = 4, range = 0 to 32
int8_t SsbSoftMuteIdx = 4;              // Default SSB = 4, range = 0 to 32

// Status bar icon flags
bool eeprom_wr_flag = false;            // Flag indicating EEPROM write request

// Firmware controlled mute
uint8_t mute_vol_val = 0;               // Volume level when mute is applied

// Menu options
int16_t currentCAL = 0;                 // Calibration offset, +/- 1000Hz in steps of 10Hz
uint16_t currentBrt = 128;              // Display brightness, range = 32 to 255 in steps of 32
int8_t currentAVC = 48;                 // Selected AVC, range = 12 to 90 in steps of 2

// Background screen refresh
uint32_t background_timer = millis();   // Background screen refresh timer.
uint32_t tuning_timer = millis();       // Tuning hold off timer.
bool tuning_flag = false;               // Flag to indicate tuning

// Bandwidth data structure
typedef struct
{
	uint8_t idx;      // SI473X device bandwidth index
	const char* desc; // bandwidth description
} Bandwidth;

int8_t bwIdxSSB = 4;
const int8_t maxSsbBw = 5;
Bandwidth bandwidthSSB[] = {
  {4, ""},
  {5, ""},
  {0, ""},
  {1, ""},
  {2, ""},
  {3, ""}
};
const int lastBandwidthSSB = (sizeof bandwidthSSB / sizeof(Bandwidth)) - 1;

int8_t bwIdxAM = 4;
const int8_t maxAmBw = 6;
Bandwidth bandwidthAM[] = {
  {4, ""},
  {5, ""},
  {3, ""},
  {6, ""},
  {2, ""},
  {1, ""},
  {0, ""}
};
const int lastBandwidthAM = (sizeof bandwidthAM / sizeof(Bandwidth)) - 1;

int8_t bwIdxFM = 0;
const int8_t maxFmBw = 4;
Bandwidth bandwidthFM[] = {
	{0, ""}, // Automatic - default
	{1, ""}, // Force wide (110 kHz) channel filter.
	{2, ""},
	{3, ""},
	{4, ""} };
const int lastBandwidthFM = (sizeof bandwidthFM / sizeof(Bandwidth)) - 1;


static constexpr const char* bandwidthSSBStr = "0.5 kHz\n1.0 kHz\n1.2 kHz\n2.2 kHz\n3.0 kHz\n4.0 kHz";
static constexpr const char* bandwidthAMStr = "1.0 kHz\n1.8 kHz\n2.0 kHz\n2.5 kHz\n3.0 kHz\n4.0 kHz\n6.0 kHz";
static constexpr const char* bandwidthFMStr = "AUTO\n110 kHz\n 84 kHz\n 60 kHz\n 40 kHz";


int tabAmStep[] = { 1,      // 0   AM/SSB   (kHz)
				   5,      // 1   AM/SSB   (kHz)
				   9,      // 2   AM/SSB   (kHz)
				   10,     // 3   AM/SSB   (kHz)
				   50,     // 4   AM       (kHz)
				   100,    // 5   AM       (kHz)
				   1000,   // 6   AM       (kHz)
				   10,     // 7   SSB      (Hz)
				   25,     // 8   SSB      (Hz)
				   50,     // 9   SSB      (Hz)
				   100,    // 10  SSB      (Hz)
				   500 };   // 11  SSB      (Hz)

uint8_t AmTotalSteps = 7;                          // Total AM steps
uint8_t AmTotalStepsSsb = 4;                       // G8PTN: Original : AM(LW/MW) 1k, 5k, 9k, 10k, 50k        : SSB 1k, 5k, 9k, 10k
//uint8_t AmTotalStepsSsb = 5;                     // G8PTN: Option 1 : AM(LW/MW) 1k, 5k, 9k, 10k, 100k       : SSB 1k, 5k, 9k, 10k, 50k
//uint8_t AmTotalStepsSsb = 6;                     // G8PTN: Option 2 : AM(LW/MW) 1k, 5k, 9k, 10k, 100k , 1M  : SSB 1k, 5k, 9k, 10k, 50k, 100k
//uint8_t AmTotalStepsSsb = 7;                     // G8PTN: Invalid option (Do not use)
uint8_t SsbTotalSteps = 5;                         // SSB sub 1kHz steps 
volatile int8_t idxAmStep = 3;

const char* AmSsbStepStr = "1 kHz\n5 kHz\n9 kHz\n10 kHz\n50 kHz\n100 kHz\n1 MHz\n10 Hz\n25 Hz\n50 Hz\n100 Hz\n500 Hz";
const char* AmStepStr = "1 kHz\n5 kHz\n9 kHz\n10 kHz\n50 kHz\n100 kHz\n1 MHz";

int tabFmStep[] = { 5, 10, 20, 100 };                             // G8PTN: Added 1MHz step
const int lastFmStep = (sizeof tabFmStep / sizeof(int)) - 1;
int idxFmStep = 1;

const char* FmStepStr = "50 kHz\n100 kHz\n200 kHz\n1 MHz";


uint16_t currentStepIdx = 1;

uint8_t currentMode = FM;

const char* modeStr = "FM\nLSB\nUSB\nAM\nLW";

const char* getStr(const char* str, int idx) {
	return getStrValue(str, idx);
};

/**
 *  Band data structure
 */
typedef struct
{
	const char* bandName;   // Band description
	uint8_t 	bandType;   // Band type (FM, MW or SW)
	uint8_t 	bandMODE; 	// Pref. modulation
	uint16_t minimumFreq;   // Minimum frequency of the band
	uint16_t maximumFreq;   // maximum frequency of the band
	uint16_t currentFreq;   // Default frequency or current frequency
	int8_t currentStepIdx;  // Idex of tabStepAM:  Defeult frequency step (See tabStepAM)
	int8_t bandwidthIdx;    // Index of the table bandwidthFM, bandwidthAM or bandwidthSSB;
	int16_t bandCAL;        // Calibration offset, +/- 1000Hz in steps of 10Hz
} Band;

/*
   Band table
   YOU CAN CONFIGURE YOUR OWN BAND PLAN. Be guided by the comments.
   To add a new band, all you have to do is insert a new line in the table below. No extra code will be needed.
   You can remove a band by deleting a line if you do not want a given band.
   Also, you can change the parameters of the band.
   ATTENTION: You have to RESET the eeprom after adding or removing a line of this table.
			  Turn your receiver on with the encoder push button pressed at first time to RESET the eeprom content.
*/

Band band[] = {
	{  "FM" 		, FM_BAND_TYPE,  FM,  8750, 10800,  9740,  1, 0, 0}, // FM        
	{  "LW" 		, LW_BAND_TYPE,  AM,   130,   279,   198,  0, 4, 0}, // LW        
	{  "AM" 		, MW_BAND_TYPE,  AM,   522,  1701,  1395,  0, 4, 0}, // AM        
	{  "Whole SW"	, SW_BAND_TYPE,  AM,  1730, 30000, 15500,  0, 4, 0}, // Whole SW  
	{  "Ham 600M"	, LW_BAND_TYPE,  AM,   280,   470,   284,  0, 4, 0}, // Ham  600M 
	{  "Ham 630M" 	, SW_BAND_TYPE, LSB,   472,   479,   475,  0, 4, 0}, // Ham  630M 
	{  "Ham 160M" 	, SW_BAND_TYPE, LSB,  1800,  1910,  1899,  0, 4, 0}, // Ham  160M 
	{  "Ham 80M" 	, SW_BAND_TYPE, LSB,  3500,  3800,  3630,  0, 4, 0}, // Ham   80M 
	{  "Ham 60M" 	, SW_BAND_TYPE, USB,  5330,  5410,  5375,  0, 4, 0}, // Ham   60M 
	{  "Ham 40M" 	, SW_BAND_TYPE, LSB,  7000,  7200,  7185,  0, 4, 0}, // Ham   40M 
	{  "Ham 30M" 	, SW_BAND_TYPE, USB, 10100, 10150, 10125,  0, 4, 0}, // Ham   30M 
	{  "Ham 20M" 	, SW_BAND_TYPE, USB, 14000, 14350, 14250,  0, 4, 0}, // Ham   20M 
	{  "Ham 17M" 	, SW_BAND_TYPE, USB, 18068, 18168, 18100,  0, 4, 0}, // Ham   17M 
	{  "Ham 15M" 	, SW_BAND_TYPE, USB, 21000, 21450, 21350,  0, 4, 0}, // Ham   15M 
	{  "Ham 12M" 	, SW_BAND_TYPE, USB, 24890, 24990, 24940,  0, 4, 0}, // Ham   12M 	
	{  "Ham 10M"	, SW_BAND_TYPE, USB, 28000, 30000, 28500,  0, 4, 0}, // Ham   10M 
	{  "CB" 		, SW_BAND_TYPE,  AM, 26200, 27990, 27200,  0, 4, 0}, // CB band
	{  "120M" 		, SW_BAND_TYPE,  AM,  2300,  2495,  2400,  0, 4, 0}, //      120M 
	{  "90M" 		, SW_BAND_TYPE,  AM,  3200,  3400,  3300,  0, 4, 0}, //       90M 
	{  "75M" 		, SW_BAND_TYPE,  AM,  3900,  4000,  3950,  0, 4, 0}, //       75M 
	{  "49M" 		, SW_BAND_TYPE,  AM,  5900,  6200,  6000,  0, 4, 0}, //       49M 
	{  "41M" 		, SW_BAND_TYPE,  AM,  7200,  7450,  7210,  0, 4, 0}, //       41M 
	{  "31M" 		, SW_BAND_TYPE,  AM,  9400,  9900,  9600,  0, 4, 0}, //       31M 
	{  "25M" 		, SW_BAND_TYPE,  AM, 11600, 12100, 11700,  0, 4, 0}, //       25M 
	{  "22M" 		, SW_BAND_TYPE,  AM, 13570, 13870, 13700,  0, 4, 0}, //       22M 
	{  "19M" 		, SW_BAND_TYPE,  AM, 15100, 15830, 15700,  0, 4, 0}, //       19M 
	{  "16M" 		, SW_BAND_TYPE,  AM, 17480, 17900, 17600,  0, 4, 0}, //       16M 
	{  "15M" 		, SW_BAND_TYPE,  AM, 18900, 19020, 18950,  0, 4, 0}, //       15M 
	{  "13M" 		, SW_BAND_TYPE,  AM, 21450, 21850, 21500,  0, 4, 0}, //       13M 
	{  "11M" 		, SW_BAND_TYPE,  AM, 25670, 26100, 25800,  0, 4, 0}  //       11M 	   
	
};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int bandIdx = 0;

const char* bandStr = "FM\nLW\nAM\nWhole SW\nHam 600M\nHam 630M\nHam 160M\nHam 80M\nHam 60M\nHam 40M\nHam 30M\nHam 20M\nHam 17M\nHam 15M\nHam 12M\nHam 10M\nCB\n120M\n90M\n75M\n49M\n41M\n31M\n25M\n22M\n19M\n16M\n15M\n13M\n11M";


//int tabStep[] = {1, 5, 10, 50, 100, 500, 1000};
//const int lastStep = (sizeof tabStep / sizeof(int)) - 1;


// Calibration (per band). Size needs to be the same as band[]
// Defaults
//int16_t bandCAL[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// Mode (per band). Size needs to be the same as band[] and mode needs to be appropriate for bandType
// Example bandType = FM_BAND_TYPE, bandMODE = FM. All other BAND_TYPE's, bandMODE = AM/LSB/USB
// Defaults
//uint8_t bandMODE[] = { FM, AM, AM, AM, LSB, AM, AM, LSB, AM, AM, AM, AM, USB, AM, AM, USB, AM, AM, USB, AM };

char* rdsMsg;
char* stationName;
char* rdsTime;

uint8_t rssi = 0;
uint8_t snr = 0;
uint8_t volume = DEFAULT_VOLUME;

// Menu Options
#define VOLUME       0
#define BAND         1
#define MODE         2
#define STEP         3
#define BW           4
#define MUTE         5
#define AGC_ATT      6
#define SOFTMUTE     7
#define AVC          8
#define SEEKUP       9
#define SEEKDOWN    10
#define CALIBRATION 11

const char* MenuStr = "Volume\nBand\nMode\nStep\nBandwidth\nMute\nAGC/ATTN\nSoftMute\nAVC\nSeek UP\nSeek DOWN\nCalibration\nExit";

int8_t currentMenuCmd = -1;
int8_t menuIdx = VOLUME;


/* ---------------------------------------- */
#define SAMPLES         512          // Must be a power of 2
#define SAMPLING_FREQ   30000         // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE       100          // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define NOISE           100           // Used as a crude noise filter, values below this are ignored
#define NUM_BANDS       16            // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands

// Sampling and FFT stuff
unsigned int sampling_period_us;
int bandValues[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
int peak[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
int oldBarHeights[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned long newTime;
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

hw_timer_t* timerFFT = NULL;

/**
 * brief	HAL functions to toggle EXTCOMIN pin
 */
void IRAM_ATTR hal_fft(void) {

	// Sample the audio pin
	for (int i = 0; i < SAMPLES; i++) {
		//newTime = micros();
		vReal[i] = analogRead(AUDIO_INPUT); // A conversion takes about 9.7uS on an ESP32
		vImag[i] = 0;
		//while ((micros() - newTime) < sampling_period_us) { /* chill */ }
	}

}

void hal_fft_start() {

	timerFFT = timerBegin(10000);
	timerAttachInterrupt(timerFFT, &hal_fft);
	timerAlarm(timerFFT, 10000, true, 0);
}

/* ---------------------------------------- */


void doSoftMute(int8_t v);
void doAgc(int8_t v);
void updateBFO();
void useBand();
void loadSSB();
void doAvc(int16_t v);
void selectMenuList(int8_t v);
uint8_t getStrength();


/* ---------------------------------------- */

// SSB Mode detection
bool isSSB()
{
	return currentMode > FM && currentMode < AM;    // This allows for adding CW mode as well as LSB/USB if required
}


// Generation of step value
int getSteps()
{
	if (isSSB())
	{
		if (idxAmStep >= AmTotalSteps)
			return tabAmStep[idxAmStep];            // SSB: Return in Hz used for VFO + BFO tuning

		return tabAmStep[idxAmStep] * 1000;         // SSB: Return in Hz used for VFO + BFO tuning
	}

	if (idxAmStep >= AmTotalSteps)                  // AM: Set to 0kHz if step is from the SSB Hz values 
		idxAmStep = 0;

	return tabAmStep[idxAmStep];                    // AM: Return value in KHz for SI4732 step
}


// Generate last step index
int getLastStep()
{

	if (isSSB())
		return AmTotalSteps + SsbTotalSteps - 1;
	else if (bandIdx == LW_BAND_TYPE || bandIdx == MW_BAND_TYPE)    // G8PTN; Added in place of check in doStep() for LW/MW step limit
		return AmTotalStepsSsb;
	else
		return AmTotalSteps - 1;
}

// -----------------------------------------------------------------------------------

/*
   writes the conrrent receiver information into the eeprom.
   The EEPROM.update avoid write the same data in the same memory position. It will save unnecessary recording.
*/
void saveAllReceiverInformation()
{
	eeprom_wr_flag = true;
	int addr_offset;
	int16_t currentBFOs = (currentBFO % 1000);            // G8PTN: For SSB ensures BFO value is valid wrt band[bandIdx].currentFreq = currentFrequency;

	EEPROM.begin(EEPROM_SIZE);

	EEPROM.write(eeprom_address, app_id);                 // Stores the app id;
	EEPROM.write(eeprom_address + 1, rx.getVolume());     // Stores the current Volume
	EEPROM.write(eeprom_address + 2, bandIdx);            // Stores the current band
	EEPROM.write(eeprom_address + 3, fmRDS);              // G8PTN: Not used
	EEPROM.write(eeprom_address + 4, currentMode);        // Stores the current Mode (FM / AM / LSB / USB). Now per mode, leave for compatibility
	EEPROM.write(eeprom_address + 5, currentBFOs >> 8);   // G8PTN: Stores the current BFO % 1000 (HIGH byte)
	EEPROM.write(eeprom_address + 6, currentBFOs & 0XFF); // G8PTN: Stores the current BFO % 1000 (LOW byte)
	EEPROM.commit();

	addr_offset = 7;

	// G8PTN: Commented out the assignment
	// - The line appears to be required to ensure the band[bandIdx].currentFreq = currentFrequency
	// - Updated main code to ensure that this should occur as required with frequency, band or mode changes
	// - The EEPROM reset code now calls saveAllReceiverInformation(), which is the correct action, this line
	//   must be disabled otherwise band[bandIdx].currentFreq = 0 (where bandIdx = 0; by default) on EEPROM reset
	//band[bandIdx].currentFreq = currentFrequency;

	for (int i = 0; i <= lastBand; i++)
	{
		EEPROM.write(addr_offset++, (band[i].currentFreq >> 8));   // Stores the current Frequency HIGH byte for the band
		EEPROM.write(addr_offset++, (band[i].currentFreq & 0xFF)); // Stores the current Frequency LOW byte for the band
		EEPROM.write(addr_offset++, band[i].currentStepIdx);       // Stores current step of the band
		EEPROM.write(addr_offset++, band[i].bandwidthIdx);         // table index (direct position) of bandwidth
		EEPROM.commit();
	}

	// G8PTN: Added
	addr_offset = eeprom_set_address;
	EEPROM.write(addr_offset++, currentBrt >> 8);         // Stores the current Brightness value (HIGH byte)
	EEPROM.write(addr_offset++, currentBrt & 0XFF);       // Stores the current Brightness value (LOW byte)
	EEPROM.write(addr_offset++, FmAgcIdx);                // Stores the current FM AGC/ATTN index value
	EEPROM.write(addr_offset++, AmAgcIdx);                // Stores the current AM AGC/ATTN index value
	EEPROM.write(addr_offset++, SsbAgcIdx);               // Stores the current SSB AGC/ATTN index value
	EEPROM.write(addr_offset++, AmAvcIdx);                // Stores the current AM AVC index value
	EEPROM.write(addr_offset++, SsbAvcIdx);               // Stores the current SSB AVC index value
	EEPROM.write(addr_offset++, AmSoftMuteIdx);           // Stores the current AM SoftMute index value
	EEPROM.write(addr_offset++, SsbSoftMuteIdx);          // Stores the current SSB SoftMute index value
	EEPROM.commit();

	addr_offset = eeprom_setp_address;
	for (int i = 0; i <= lastBand; i++)
	{
		EEPROM.write(addr_offset++, (band[i].bandCAL >> 8));     // Stores the current Calibration value (HIGH byte) for the band
		EEPROM.write(addr_offset++, (band[i].bandCAL & 0XFF));   // Stores the current Calibration value (LOW byte) for the band
		EEPROM.write(addr_offset++, band[i].bandMODE);          // Stores the current Mode value for the band
		EEPROM.commit();
	}

	addr_offset = eeprom_ver_address;
	EEPROM.write(addr_offset++, app_ver >> 8);            // Stores app_ver (HIGH byte)
	EEPROM.write(addr_offset++, app_ver & 0XFF);          // Stores app_ver (LOW byte)
	EEPROM.commit();

	EEPROM.end();
}


/**
 * reads the last receiver status from eeprom.
 */
void readAllReceiverInformation()
{
	uint8_t volume;
	int addr_offset;
	int bwIdx;
	EEPROM.begin(EEPROM_SIZE);

	volume = EEPROM.read(eeprom_address + 1); // Gets the stored volume;
	bandIdx = EEPROM.read(eeprom_address + 2);
	fmRDS = EEPROM.read(eeprom_address + 3);                // G8PTN: Not used
	currentMode = EEPROM.read(eeprom_address + 4);          // G8PTM: Reads stored Mode. Now per mode, leave for compatibility
	currentBFO = EEPROM.read(eeprom_address + 5) << 8;      // G8PTN: Reads stored BFO value (HIGH byte)
	currentBFO |= EEPROM.read(eeprom_address + 6);          // G8PTN: Reads stored BFO value (HIGH byte)

	addr_offset = 7;
	for (int i = 0; i <= lastBand; i++)
	{
		band[i].currentFreq = EEPROM.read(addr_offset++) << 8;
		band[i].currentFreq |= EEPROM.read(addr_offset++);
		band[i].currentStepIdx = EEPROM.read(addr_offset++);
		band[i].bandwidthIdx = EEPROM.read(addr_offset++);
	}

	// G8PTN: Added
	addr_offset = eeprom_set_address;
	currentBrt = EEPROM.read(addr_offset++) << 8;      // Reads stored Brightness value (HIGH byte)
	currentBrt |= EEPROM.read(addr_offset++);           // Reads stored Brightness value (LOW byte)
	FmAgcIdx = EEPROM.read(addr_offset++);           // Reads stored FM AGC/ATTN index value
	AmAgcIdx = EEPROM.read(addr_offset++);           // Reads stored AM AGC/ATTN index value
	SsbAgcIdx = EEPROM.read(addr_offset++);           // Reads stored SSB AGC/ATTN index value
	AmAvcIdx = EEPROM.read(addr_offset++);           // Reads stored AM AVC index value
	SsbAvcIdx = EEPROM.read(addr_offset++);           // Reads stored SSB AVC index value
	AmSoftMuteIdx = EEPROM.read(addr_offset++);           // Reads stored AM SoftMute index value
	SsbSoftMuteIdx = EEPROM.read(addr_offset++);           // Reads stored SSB SoftMute index value

	addr_offset = eeprom_setp_address;
	for (int i = 0; i <= lastBand; i++)
	{
		band[i].bandCAL = EEPROM.read(addr_offset++) << 8;      // Reads stored Calibration value (HIGH byte) per band
		band[i].bandCAL |= EEPROM.read(addr_offset++);           // Reads stored Calibration value (LOW byte) per band
		band[i].bandMODE = EEPROM.read(addr_offset++);           // Reads stored Mode value per band
	}

	EEPROM.end();

	currentFrequency = band[bandIdx].currentFreq;
	currentMode = band[bandIdx].bandMODE;                       // G8PTN: Added to support mode per band

	if (band[bandIdx].bandType == FM_BAND_TYPE)
	{
		currentStepIdx = idxFmStep = band[bandIdx].currentStepIdx;
		rx.setFrequencyStep(tabFmStep[currentStepIdx]);
	}
	else
	{
		currentStepIdx = band[bandIdx].currentStepIdx;
		idxAmStep = currentStepIdx;
		rx.setFrequencyStep(tabAmStep[currentStepIdx]);
	}

	bwIdx = band[bandIdx].bandwidthIdx;

	if (isSSB())
	{
		loadSSB();
		bwIdxSSB = (bwIdx > 5) ? 5 : bwIdx;
		rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
		// If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
		if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
			rx.setSSBSidebandCutoffFilter(0);
		else
			rx.setSSBSidebandCutoffFilter(1);
		updateBFO();
	}
	else if (currentMode == AM)
	{
		bwIdxAM = bwIdx;
		rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
	}
	else
	{
		bwIdxFM = bwIdx;
		rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
	}

	delay(50);
	rx.setVolume(volume);
}

/*
 * To store any change into the EEPROM, it is needed at least STORE_TIME  milliseconds of inactivity.
 */
void resetEepromDelay()
{
	elapsedCommand = storeTime = millis();
	itIsTimeToSave = true;
}

void showInfoMsg(const char* msg) {
	strncpy(infoMessage, msg, sizeof(infoMessage) - 1);
	infoMessage[sizeof(infoMessage) - 1] = '\0'; // Ensure null termination
	infoShow = true;
	elapsedCommand = millis();
}


/**
	Set all command flags to false
	When all flags are disabled (false), the encoder controls the frequency
*/
void disableCommands()
{
	cmdBand = false;
	bfoOn = false;
	cmdVolume = false;
	cmdAgc = false;
	cmdBandwidth = false;
	cmdStep = false;
	cmdMode = false;
	cmdMenu = false;
	cmdSoftMuteMaxAtt = false;
	countClick = 0;
	cmdCal = false;
	cmdAvc = false;

}

void drawMainVFO() {
	ui.clearMain();

	ui.drawFrequencyBig(currentFrequency, currentBFO, band[bandIdx].bandType, currentMode, 270, 105);

	ui.draw_ic_mode(320, 70, BLACK);

	ui.setFont(Font::FONT_B20_TF);
	ui.drawString(TextAlign::CENTER, 318, 368, 94, false, false, false, getStr(modeStr, currentMode));

	ui.setFont(Font::FONT_20_TF);

	ui.draw_ic24_step(20, 35, BLACK);
	if (band[bandIdx].bandType == FM_BAND_TYPE) {
		ui.drawString(TextAlign::LEFT, 55, 0, 48, true, true, false, getStr(FmStepStr, currentStepIdx));
	}
	else {
		ui.drawString(TextAlign::LEFT, 55, 0, 48, true, true, false, getStr(AmSsbStepStr, currentStepIdx));
	}

	ui.draw_ic24_bandwidth(150, 28, BLACK);
	if (isSSB())
	{
		ui.drawString(TextAlign::LEFT, 180, 0, 48, true, true, false, getStr(bandwidthSSBStr, bwIdxSSB));
	}
	else if (currentMode == AM)
	{
		ui.drawString(TextAlign::LEFT, 180, 0, 48, true, true, false, getStr(bandwidthAMStr, bwIdxAM));
	}
	else
	{
		ui.drawString(TextAlign::LEFT, 180, 0, 48, true, true, false, getStr(bandwidthFMStr, bwIdxFM));
	}

	ui.draw_ic24_agc(273, 32, BLACK);
	if (agcNdx == 0 && agcIdx == 0) {
		ui.drawString(TextAlign::LEFT, 300, 0, 48, true, true, false, "AGC ON");
	}
	else
	{
		ui.drawStringf(TextAlign::LEFT, 300, 0, 48, true, true, false, "ATT: %2.2d", agcNdx);
	}

	// RSSI
	ui.drawRSSI(rssi, getStrength(), snr, 40, 120);

	ui.setBlackColor();
	ui.lcd()->drawBox(0, 160, 288, 80);

	if (band[bandIdx].bandType == FM_BAND_TYPE) {
		if (rx.getCurrentPilot()) {
			ui.draw_stereo(50, 75, BLACK);
		}

		if (stationName != nullptr) {
			//ui.setBlackColor();
			ui.setFont(Font::FONT_B20_TF);
			ui.drawStringf(TextAlign::CENTER, 10, 300, 177, false, false, false, "%s", stationName);
			if (rdsMsg != nullptr) {
				ui.setFont(Font::FONT_18_TF);
				ui.draw_string_multi_line(rdsMsg, 30, 5, 195);
			}
		}

		/*if (rdsTime != nullptr) {
			ui.drawStrf(10, 180, rdsTime);
		}*/
	}
	else {
		ui.setFont(Font::FONT_B20_TF);
		ui.drawStringf(TextAlign::CENTER, 10, 300, 180, false, false, false, band[bandIdx].bandName);

		ui.setFont(Font::FONT_20_TF);
		ui.draw_start(10, 195, WHITE);
		ui.drawFrequency(band[bandIdx].minimumFreq, 30, 210);

		ui.draw_finish(10, 215, WHITE);
		ui.drawFrequency(band[bandIdx].maximumFreq, 30, 230);
	}
}

#define TOP 75
void drawSpectrum() {

	// Reset bandValues[]
	for (int i = 0; i < NUM_BANDS; i++) {
		bandValues[i] = 0;
	}

	// Sample the audio pin
	for (int i = 0; i < SAMPLES; i++) {
		//newTime = micros();
		vReal[i] = analogRead(AUDIO_INPUT); // A conversion takes about 9.7uS on an ESP32
		vImag[i] = 0;
		//while ((micros() - newTime) < sampling_period_us) { /* chill */ }
	}

	// Compute FFT
	FFT.dcRemoval();
	FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
	FFT.compute(FFT_FORWARD);
	FFT.complexToMagnitude();

	// Analyse FFT results
	for (int i = 2; i < (SAMPLES / 2); i++) {       // Don't use sample 0 and only first SAMPLES/2 are usable. Each array element represents a frequency bin and its value the amplitude.
		if (vReal[i] > NOISE) {                    // Add a crude noise filter

			//16 bands, 12kHz top band
			if (i <= 2)           bandValues[0] += (int)vReal[i];
			if (i > 2 && i <= 3) bandValues[1] += (int)vReal[i];
			if (i > 3 && i <= 5) bandValues[2] += (int)vReal[i];
			if (i > 5 && i <= 7) bandValues[3] += (int)vReal[i];
			if (i > 7 && i <= 9) bandValues[4] += (int)vReal[i];
			if (i > 9 && i <= 13) bandValues[5] += (int)vReal[i];
			if (i > 13 && i <= 18) bandValues[6] += (int)vReal[i];
			if (i > 18 && i <= 25) bandValues[7] += (int)vReal[i];
			if (i > 25 && i <= 36) bandValues[8] += (int)vReal[i];
			if (i > 36 && i <= 50) bandValues[9] += (int)vReal[i];
			if (i > 50 && i <= 69) bandValues[10] += (int)vReal[i];
			if (i > 69 && i <= 97) bandValues[11] += (int)vReal[i];
			if (i > 97 && i <= 135) bandValues[12] += (int)vReal[i];
			if (i > 135 && i <= 189) bandValues[13] += (int)vReal[i];
			if (i > 189 && i <= 264) bandValues[14] += (int)vReal[i];
			if (i > 264) bandValues[15] += (int)vReal[i];
		}
	}

	// Process the FFT data into bar heights

	ui.setBlackColor();
	ui.lcd()->drawBox(288, 160, 112, 80);

	ui.setWhiteColor();
	ui.lcd()->drawVLine(288, 160, 80);

	for (byte band = 0; band < NUM_BANDS - 1; band++) {
		// Scale the bars for the display
		int barHeight = bandValues[band] / ui.map(rx.getVolume(), 0, 63, 1000, 2000);
		if (barHeight > TOP) barHeight = TOP;

		// Small amount of averaging between frames
		barHeight = ((oldBarHeights[band] * 1) + barHeight) / 2;

		// Move peak up
		if (barHeight > peak[band]) {
			peak[band] = min(TOP, barHeight);
		}

		// Draw the bars		
		//ui.lcd()->drawVLine(300 + (band * 4), 240 - barHeight, barHeight);
		ui.lcd()->drawBox(390 - (band * 7), 240 - barHeight, 6, barHeight);
		//ui.lcd()->drawFrame(390 - (band * 6), 240 - TOP, 5, TOP);

		ui.lcd()->drawBox(390 - (band * 7), 240 - peak[band] - 2, 6, 2);

		oldBarHeights[band] = barHeight;
	}

	//EVERY_N_MILLISECONDS(60) {
	for (byte band = 0; band < NUM_BANDS - 1; band++)
		if (peak[band] > 0) peak[band] -= 1;
	//}
}

/**
 * Shows some basic information on display
 */
void showStatus()
{
	ui.drawStatus(rx.getVolume(), itIsTimeToSave);

}

void cleanBfoRdsInfo()
{
	rdsMsg = nullptr;
	stationName = nullptr;
	rdsTime = nullptr;
}

void showMenuScreen(const char* name, uint8_t h) {
	int posX = 205;
	int posY = 28;
	int mW = 180;

	ui.setWhiteColor();
	ui.lcd()->drawRBox(posX - 3, posY - 3, mW + 6, h + 6, 8);

	ui.setBlackColor();
	ui.lcd()->drawRBox(posX, 28, mW, 24, 8);

	ui.setWhiteColor();
	ui.lcd()->drawBox(posX, posY + 21, mW, h - 21);

	ui.setBlackColor();
	ui.lcd()->drawRFrame(posX, posY, mW, h, 8);
	ui.lcd()->drawRFrame(posX + 2, posY, mW - 4, h - 2, 8);

	ui.setFont(Font::FONT_B20_TF);
	ui.drawString(TextAlign::CENTER, posX + 1, posX + mW - 2, posY + 18, false, false, false, name);

}

/**
 *   Sets Band up (1) or down (!1)
 */
void setBand(int8_t up_down)
{
	// G8PTN: Reset BFO when changing band and store frequency
	band[bandIdx].currentFreq = currentFrequency + (currentBFO / 1000);
	currentBFO = 0;

	band[bandIdx].currentStepIdx = currentStepIdx;
	if (up_down == -1)                                            // G8PTN: Corrected direction
		bandIdx = (bandIdx < lastBand) ? (bandIdx + 1) : 0;
	else
		bandIdx = (bandIdx > 0) ? (bandIdx - 1) : lastBand;

	// G8PTN: Added to support mode per band
	currentMode = band[bandIdx].bandMODE;

	ui.setListPos(bandIdx);

	if (isSSB())
	{
		if (ssbLoaded == false)
		{
			// Only loadSSB if not already loaded      
			ui.drawLoading();
			ui.updateDisplay();

			loadSSB();
			ssbLoaded = true;
		}
	}
	else {
		// If not SSB
		ssbLoaded = false;
	}

	useBand();
	delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
	elapsedCommand = millis();
}


/**
 * Switch the radio to current band
 */
void useBand()
{
	currentMode = band[bandIdx].bandMODE;                  // G8PTN: Added to support mode per band
	if (band[bandIdx].bandType == FM_BAND_TYPE)
	{
		currentMode = FM;
		rx.setTuneFrequencyAntennaCapacitor(0);
		rx.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, tabFmStep[band[bandIdx].currentStepIdx]);
		rx.setSeekFmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
		bfoOn = ssbLoaded = false;
		bwIdxFM = band[bandIdx].bandwidthIdx;
		rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
		rx.setFMDeEmphasis(1);
		rx.RdsInit();
		rx.setRdsConfig(1, 2, 2, 2, 2);
		rx.setGpioCtl(1, 0, 0);   // G8PTN: Enable GPIO1 as output
		rx.setGpio(0, 0, 0);      // G8PTN: Set GPIO1 = 0
	}
	else
	{
		// set the tuning capacitor for SW or MW/LW
		rx.setTuneFrequencyAntennaCapacitor((band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) ? 0 : 1);
		if (ssbLoaded)
		{
			// Configure SI4732 for SSB
			rx.setSSB(
				band[bandIdx].minimumFreq,
				band[bandIdx].maximumFreq,
				band[bandIdx].currentFreq,
				0,                                                  // SI4732 step is not used for SSB! 
				currentMode);

			rx.setSSBAutomaticVolumeControl(1);                   // G8PTN: Always enabled
			//rx.setSsbSoftMuteMaxAttenuation(softMuteMaxAttIdx); // G8PTN: Commented out
			if (band[bandIdx].bandwidthIdx > 5) bwIdxSSB = 5;   // G8PTN: Limit value
			else bwIdxSSB = band[bandIdx].bandwidthIdx;
			rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
			updateBFO();                                          // G8PTN: If SSB is loaded update BFO
		}
		else
		{
			currentMode = AM;
			rx.setAM(
				band[bandIdx].minimumFreq,
				band[bandIdx].maximumFreq,
				band[bandIdx].currentFreq,
				band[bandIdx].currentStepIdx >= AmTotalSteps ? 1 : tabAmStep[band[bandIdx].currentStepIdx]);   // Set to 1kHz

			bfoOn = false;
			bwIdxAM = band[bandIdx].bandwidthIdx;
			rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
			//rx.setAmSoftMuteMaxAttenuation(softMuteMaxAttIdx); //Soft Mute for AM or SSB
		}
		rx.setGpioCtl(1, 0, 0);   // G8PTN: Enable GPIO1 as output
		rx.setGpio(1, 0, 0);      // G8PTN: Set GPIO1 = 1
		rx.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq); // Consider the range all defined current band
		rx.setSeekAmSpacing(5); // Max 10kHz for spacing

	}

	// G8PTN: Added
	// Call doSoftMute(0), 0 = No incr/decr action (eqivalent to getSoftMute)
	// This gets softMuteMaxAttIdx based on mode (AM, SSB)  
	doSoftMute(0);

	// Call doAgc(0), 0 = No incr/decr action (eqivalent to getAgc)
	// This gets disableAgc and agcNdx values based on mode (FM, AM , SSB)  
	doAgc(0);

	// Call doAvc(0), 0 = No incr/decr action (eqivalent to getAvc)
	// This gets currentAVC values based on mode (AM, SSB)
	doAvc(0);

	delay(100);

	// Default
	currentFrequency = band[bandIdx].currentFreq;
	currentStepIdx = band[bandIdx].currentStepIdx;    // Default. Need to modify for AM/SSB as required


	if (currentMode == FM)
		idxFmStep = band[bandIdx].currentStepIdx;
	else
	{
		// Default for AM/SSB
		idxAmStep = band[bandIdx].currentStepIdx;


		// Update depending on currentMode and currentStepIdx
		// If outside SSB step ranges
		if (isSSB() && currentStepIdx >= AmTotalStepsSsb && currentStepIdx < AmTotalSteps)
		{
			currentStepIdx = 0;;
			idxAmStep = 0;
			band[bandIdx].currentStepIdx = 0;
		}

		// If outside AM step ranges
		if (currentMode == AM && currentStepIdx >= AmTotalSteps)
		{
			currentStepIdx = 0;;
			idxAmStep = 0;
			band[bandIdx].currentStepIdx = 0;
		}

	}

	/*
	// G8PTN: Why is this required?
	if ((bandIdx == LW_BAND_TYPE || bandIdx == MW_BAND_TYPE)
		&& idxAmStep > AmTotalStepsSsb)
		idxAmStep = AmTotalStepsSsb;
	*/

	// Store mode
	band[bandIdx].bandMODE = currentMode;               // G8PTN: Added to support mode per band

	rssi = 0;
	snr = 0;
	cleanBfoRdsInfo();
	showStatus();
}

void loadSSB() {
	rx.setI2CFastModeCustom(400000); // You can try rx.setI2CFastModeCustom(700000); or greater value
	rx.loadPatch(ssb_patch_content, size_content, bandwidthSSB[bwIdxSSB].idx);
	rx.setI2CFastModeCustom(100000);
	ssbLoaded = true;
}

/**
 *  Switches the Bandwidth
 */
void doBandwidth(int8_t v)
{
	if (isSSB())
	{
		bwIdxSSB = (v == -1) ? bwIdxSSB + 1 : bwIdxSSB - 1;

		if (bwIdxSSB > maxSsbBw)
			bwIdxSSB = 0;
		else if (bwIdxSSB < 0)
			bwIdxSSB = maxSsbBw;

		rx.setSSBAudioBandwidth(bandwidthSSB[bwIdxSSB].idx);
		// If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
		if (bandwidthSSB[bwIdxSSB].idx == 0 || bandwidthSSB[bwIdxSSB].idx == 4 || bandwidthSSB[bwIdxSSB].idx == 5)
			rx.setSSBSidebandCutoffFilter(0);
		else
			rx.setSSBSidebandCutoffFilter(1);

		band[bandIdx].bandwidthIdx = bwIdxSSB;
	}
	else if (currentMode == AM)
	{
		bwIdxAM = (v == -1) ? bwIdxAM + 1 : bwIdxAM - 1;

		if (bwIdxAM > maxAmBw)
			bwIdxAM = 0;
		else if (bwIdxAM < 0)
			bwIdxAM = maxAmBw;

		rx.setBandwidth(bandwidthAM[bwIdxAM].idx, 1);
		band[bandIdx].bandwidthIdx = bwIdxAM;

	}
	else {
		bwIdxFM = (v == -1) ? bwIdxFM + 1 : bwIdxFM - 1;
		if (bwIdxFM > maxFmBw)
			bwIdxFM = 0;
		else if (bwIdxFM < 0)
			bwIdxFM = maxFmBw;

		rx.setFmBandwidth(bandwidthFM[bwIdxFM].idx);
		band[bandIdx].bandwidthIdx = bwIdxFM;
	}

	ui.setListPos(band[bandIdx].bandwidthIdx);
	delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
}

/**
 *  AGC and attenuattion setup
 */
void doAgc(int8_t v) {

	// G8PTN: Modified to have separate AGC/ATTN per mode (FM, AM, SSB)
	if (currentMode == FM) {
		if (v == 1)   FmAgcIdx++;
		else if (v == -1)  FmAgcIdx--;

		// Limit range
		if (FmAgcIdx < 0)
			FmAgcIdx = 27;
		else if (FmAgcIdx > 27)
			FmAgcIdx = 0;

		// Select
		agcIdx = FmAgcIdx;
	}

	else if (isSSB()) {
		if (v == 1)   SsbAgcIdx++;
		else if (v == -1)  SsbAgcIdx--;

		// Limit range
		if (SsbAgcIdx < 0)
			SsbAgcIdx = 1;
		else if (SsbAgcIdx > 1)
			SsbAgcIdx = 0;

		// Select
		agcIdx = SsbAgcIdx;
	}

	else {
		if (v == 1)   AmAgcIdx++;
		else if (v == -1)  AmAgcIdx--;

		// Limit range
		if (AmAgcIdx < 0)
			AmAgcIdx = 37;
		else if (AmAgcIdx > 37)
			AmAgcIdx = 0;

		// Select
		agcIdx = AmAgcIdx;
	}

	// Process agcIdx to generate disableAgc and agcIdx
	// agcIdx     0 1 2 3 4 5 6  ..... n    (n:    FM = 27, AM = 37, SSB = 1)
	// agcNdx     0 0 1 2 3 4 5  ..... n -1 (n -1: FM = 26, AM = 36, SSB = 0) 
	// disableAgc 0 1 1 1 1 1 1  ..... 1 
	disableAgc = (agcIdx > 0);     // if true, disable AGC; else, AGC is enabled
	if (agcIdx > 1)
		agcNdx = agcIdx - 1;
	else
		agcNdx = 0;

	// Configure SI4732/5
	rx.setAutomaticGainControl(disableAgc, agcNdx); // if agcNdx = 0, no attenuation

	delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
	elapsedCommand = millis();
}

/**
 * Switches the current step
 */
void doStep(int8_t v)
{
	//selectMenuList(v);

	if (currentMode == FM) {
		idxFmStep = (v == 1) ? idxFmStep - 1 : idxFmStep + 1;
		if (idxFmStep > lastFmStep)
			idxFmStep = 0;
		else if (idxFmStep < 0)
			idxFmStep = lastFmStep;

		currentStepIdx = idxFmStep;
		rx.setFrequencyStep(tabFmStep[currentStepIdx]);
	}

	else {
		idxAmStep = (v == 1) ? idxAmStep - 1 : idxAmStep + 1;
		if (idxAmStep > getLastStep())
			idxAmStep = 0;
		else if (idxAmStep < 0)
			idxAmStep = getLastStep();

		//SSB Step limit
		/*else if (isSSB() && idxAmStep >= AmTotalStepsSsb && idxAmStep < AmTotalSteps)
			idxAmStep = v == 1 ? AmTotalSteps : AmTotalStepsSsb - 1;*/

			// G8PTN: Reduced steps for LW/MW now covered in getLastStep()
			/*
			//LW/MW Step limit
			else if ((bandIdx == LW_BAND_TYPE || bandIdx == MW_BAND_TYPE)
				&& v == 1 && idxAmStep > AmTotalStepsSsb && idxAmStep < AmTotalSteps)
				idxAmStep = AmTotalSteps;
			else if ((bandIdx == LW_BAND_TYPE || bandIdx == MW_BAND_TYPE)
				&& v != 1 && idxAmStep > AmTotalStepsSsb && idxAmStep < AmTotalSteps)
				idxAmStep = AmTotalStepsSsb;
			*/

		if (!isSSB() || isSSB() && idxAmStep < AmTotalSteps)
		{
			currentStepIdx = idxAmStep;
			rx.setFrequencyStep(tabAmStep[idxAmStep]);
		}

		/*
		if (!isSSB())
			rx.setSeekAmSpacing((band[bandIdx].currentStepIdx >= AmTotalSteps) ? 1 : tabStep[band[bandIdx].currentStepIdx]);
		*/

		currentStepIdx = idxAmStep;
		//rx.setFrequencyStep(tabAmStep[currentStepIdx]);
		rx.setSeekAmSpacing(5); // Max 10kHz for spacing
	}

	band[bandIdx].currentStepIdx = currentStepIdx;
	ui.setListPos(currentStepIdx);
	elapsedCommand = millis();
}

/**
 * Switches to the AM, LSB or USB modes
 */
void doMode(int8_t v)
{
	currentMode = band[bandIdx].bandMODE;               // G8PTN: Added to support mode per band

	if (currentMode != FM)                         // Nothing to do if FM mode
	{
		if (v == -1) {
			if (currentMode == AM)
			{
				// If you were in AM mode, it is necessary to load SSB patch (every time)

				ui.drawLoading();
				ui.updateDisplay();

				loadSSB();
				ssbLoaded = true;
				currentMode = LSB;
			}
			else if (currentMode == LSB)
				currentMode = USB;
			else if (currentMode == USB)
			{
				currentMode = AM;
				bfoOn = ssbLoaded = false;

				// G8PTN: When exiting SSB mode update the current frequency and BFO
				currentFrequency = currentFrequency + (currentBFO / 1000);
				currentBFO = 0;
			}
		}
		else {
			if (currentMode == AM)
			{
				// If you were in AM mode, it is necessary to load SSB patch (every time)

				ui.drawLoading();
				ui.updateDisplay();

				loadSSB();
				ssbLoaded = true;
				currentMode = USB;
			}
			else if (currentMode == USB)
				currentMode = LSB;
			else if (currentMode == LSB)
			{
				currentMode = AM;
				bfoOn = ssbLoaded = false;

				// G8PTN: When exiting SSB mode update the current frequency and BFO
				currentFrequency = currentFrequency + (currentBFO / 1000);
				currentBFO = 0;
			}
		}

		band[bandIdx].currentFreq = currentFrequency;
		band[bandIdx].currentStepIdx = currentStepIdx;
		band[bandIdx].bandMODE = currentMode;                      // G8PTN: Added to support mode per band
		useBand();
		ui.setListPos(currentMode);
	}
	delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
	elapsedCommand = millis();
}

/**
 * Sets the audio volume
 */
void doVolume(int8_t v) {
	if (v == 1)
		rx.volumeUp();
	else
		rx.volumeDown();

	delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
}

/**
 *  This function is called by the seek function process.  G8PTN: Added
 */
bool checkStopSeeking() {
	// Checks the seekStop flag
	return seekStop;  // returns true if the user rotates the encoder
}

/**
 *  This function is called by the seek function process.
 */
void showFrequencySeek(uint16_t freq)
{
	currentFrequency = freq;
}

/**
 *  Find a station. The direction is based on the last encoder move clockwise or counterclockwise
 */
void doSeek()
{
	if (isSSB()) return; // It does not work for SSB mode

	rx.seekStationProgress(showFrequencySeek, checkStopSeeking, seekDirection);   // G8PTN: Added checkStopSeeking
	currentFrequency = rx.getFrequency();

}

/**
 * Sets the Soft Mute Parameter
 */
void doSoftMute(int8_t v)
{
	// G8PTN: Modified to have separate SoftMute per mode (AM, SSB)
	// Only allow for AM and SSB modes
	if (currentMode != FM) {

		if (isSSB()) {
			if (v == 1)   SsbSoftMuteIdx++;
			else if (v == -1)  SsbSoftMuteIdx--;

			// Limit range
			if (SsbSoftMuteIdx < 0)
				SsbSoftMuteIdx = 32;
			else if (SsbSoftMuteIdx > 32)
				SsbSoftMuteIdx = 0;

			// Select
			softMuteMaxAttIdx = SsbSoftMuteIdx;
		}

		else {
			if (v == 1)   AmSoftMuteIdx++;
			else if (v == -1)  AmSoftMuteIdx--;

			// Limit range
			if (AmSoftMuteIdx < 0)
				AmSoftMuteIdx = 32;
			else if (AmSoftMuteIdx > 32)
				AmSoftMuteIdx = 0;

			// Select
			softMuteMaxAttIdx = AmSoftMuteIdx;
		}

		rx.setAmSoftMuteMaxAttenuation(softMuteMaxAttIdx);

		// Only call showSoftMute() if incr/decr action (allows the doSoftMute(0) to act as getSoftMute)
		//if (v != 0) showSoftMute();

		elapsedCommand = millis();
	}
}

/**
 *  Menu options selection
 */
void doMenu(int8_t v) {

	selectMenuList(v);

	menuIdx = ui.getListPos();
}

void selectMenuList(int8_t v) {

	if (v > 0) {
		ui.listPrev();
	}
	else {
		ui.listNext();
	}
}

/**
 * Return true if the current status is Menu command
 */
bool isMenuMode() {
	return (cmdMenu | cmdStep | cmdBandwidth | cmdAgc | cmdVolume | cmdSoftMuteMaxAtt | cmdMode | cmdBand | cmdCal | cmdAvc);     // G8PTN: Added cmdBand, cmdCal, cmdBrt and cmdAvc
}


uint8_t getStrength() {
	if (currentMode != FM) {
		//dBuV to S point conversion HF
		if ((rssi >= 0) and (rssi <= 1)) 	return  1;  // S0
		if ((rssi > 1) 	and (rssi <= 2)) 	return  2;  // S1         // G8PTN: Corrected table
		if ((rssi > 2) 	and (rssi <= 3)) 	return  3;  // S2
		if ((rssi > 3) 	and (rssi <= 4)) 	return  4;  // S3
		if ((rssi > 4) 	and (rssi <= 10)) 	return  5;  // S4
		if ((rssi > 10) and (rssi <= 16)) 	return  6;  // S5
		if ((rssi > 16) and (rssi <= 22)) 	return  7;  // S6
		if ((rssi > 22) and (rssi <= 28)) 	return  8;  // S7
		if ((rssi > 28) and (rssi <= 34)) 	return  9;  // S8
		if ((rssi > 34) and (rssi <= 44)) 	return 10;  // S9
		if ((rssi > 44) and (rssi <= 54)) 	return 11;  // S9 +10
		if ((rssi > 54) and (rssi <= 64)) 	return 12;  // S9 +20
		if ((rssi > 64) and (rssi <= 74)) 	return 13;  // S9 +30
		if ((rssi > 74) and (rssi <= 84)) 	return 14;  // S9 +40
		if ((rssi > 84) and (rssi <= 94)) 	return 15;  // S9 +50
		if (rssi > 94)                   	return 16;  // S9 +60
		if (rssi > 95)                   	return 17;  //>S9 +60
	}
	else
	{
		//dBuV to S point conversion FM
		if (rssi >= 0 	and (rssi <= 1)) 	return  1;               // G8PTN: Corrected table
		if ((rssi > 1) 	and (rssi <= 2)) 	return  7;  // S6
		if ((rssi > 2) 	and (rssi <= 8)) 	return  8;  // S7
		if ((rssi > 8) 	and (rssi <= 14)) 	return  9;  // S8
		if ((rssi > 14) and (rssi <= 24)) 	return 10;  // S9
		if ((rssi > 24) and (rssi <= 34)) 	return 11;  // S9 +10
		if ((rssi > 34) and (rssi <= 44)) 	return 12;  // S9 +20
		if ((rssi > 44) and (rssi <= 54)) 	return 13;  // S9 +30
		if ((rssi > 54) and (rssi <= 64)) 	return 14;  // S9 +40
		if ((rssi > 64) and (rssi <= 74)) 	return 15;  // S9 +50
		if (rssi > 74)                   	return 16;  // S9 +60
		if (rssi > 76)                   	return 17;  //>S9 +60
	}

	return 0;
}

void checkRDS()
{
	char* sName = nullptr;
	char* sInfo = nullptr;
	char* sTime = nullptr;

	rx.getRdsStatus();
	if (rx.getRdsReceived())
	{
		if (rx.getRdsSync() && rx.getRdsSyncFound())
		{
			//sName = rx.getRdsText2A();
			sName = rx.getRdsStationName();
			if (sName != nullptr) {
				stationName = sName;
			}
			//sInfo = rx.getRdsText0A();
			sInfo = rx.getRdsProgramInformation();
			if (sInfo != nullptr) {
				rdsMsg = sInfo;
			}
			sTime = rx.getRdsTime();
			if (sTime != nullptr) {
				rdsTime = sTime;
			}
		}
	}
}

/***************************************************************************************
** Description:   In SSB mode tuning uses VFO and BFO
**                - Algorithm from ATS-20_EX Goshante firmware
***************************************************************************************/
// Tuning algorithm

void updateBFO()
{
	// To move frequency forward, need to move the BFO backwards, so multiply by -1
	currentCAL = band[bandIdx].bandCAL;    // Select from table
	rx.setSSBBfo((currentBFO + currentCAL) * -1);

}

// Clamp SSB tuning to band limits
bool clampSSBBand()
{
	uint16_t freq = currentFrequency + (currentBFO / 1000);

	// Special case to cover SSB frequency negative!
	bool SsbFreqNeg = false;
	if (currentFrequency & 0x8000)
		SsbFreqNeg = true;

	// Priority to minimum check to cover SSB frequency negative
	bool upd = false;
	if (freq < band[bandIdx].minimumFreq || SsbFreqNeg)
	{
		currentFrequency = band[bandIdx].maximumFreq;
		upd = true;
	}
	else if (freq > band[bandIdx].maximumFreq)
	{
		currentFrequency = band[bandIdx].minimumFreq;
		upd = true;
	}

	if (upd)
	{
		band[bandIdx].currentFreq = currentFrequency;    // Update band table currentFreq
		rx.setFrequency(currentFrequency);
		currentBFO = 0;
		updateBFO();
		return true;
	}

	return false;
}

void doFrequencyTuneSSB()
{
	//const int BFOMax = 16000;    G8PTN: Moved to a global variable
	int step = encoderCount == 1 ? getSteps() : getSteps() * -1;
	int newBFO = currentBFO + step;
	int redundant = 0;

	if (newBFO > BFOMax)
	{
		redundant = (newBFO / BFOMax) * BFOMax;
		currentFrequency += redundant / 1000;
		newBFO -= redundant;
	}
	else if (newBFO < -BFOMax)
	{
		redundant = ((abs(newBFO) / BFOMax) * BFOMax);
		currentFrequency -= redundant / 1000;
		newBFO += redundant;
	}

	currentBFO = newBFO;
	updateBFO();

	if (redundant != 0)

	{
		clampSSBBand();                                   // G8PTN: Added          
		rx.setFrequency(currentFrequency);
		//agcSetFunc(); //Re-apply to remove noize        // G8PTN: Commented out
		currentFrequency = rx.getFrequency();
		//band[bandIdx].currentFreq = currentFrequency;   // G8PTN: Commented out, covered below
	}

	band[bandIdx].currentFreq = currentFrequency + (currentBFO / 1000);     // Update band table currentFreq

}

void doCal(int16_t v) {
	currentCAL = band[bandIdx].bandCAL;    // Select from table
	if (v == 1) {
		currentCAL = currentCAL + 10;
		if (currentCAL > CALMax) currentCAL = CALMax;
	}

	else {
		currentCAL = currentCAL - 10;
		if (currentCAL < -CALMax) currentCAL = -CALMax;
	}
	band[bandIdx].bandCAL = currentCAL;    // Store to table

	// If in SSB mode set the SI4732/5 BFO value
	// This adjustments the BFO whilst in the calibration menu
	if (isSSB()) updateBFO();

	delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
}

void doAvc(int16_t v) {
	// Only allow for AM and SSB modes
	if (currentMode != FM) {

		if (isSSB()) {
			if (v == 1)   SsbAvcIdx += 2;
			else if (v == -1)  SsbAvcIdx -= 2;

			// Limit range
			if (SsbAvcIdx < 12)
				SsbAvcIdx = 90;
			else if (SsbAvcIdx > 90)
				SsbAvcIdx = 12;

			// Select
			currentAVC = SsbAvcIdx;
		}

		else {
			if (v == 1)   AmAvcIdx += 2;
			else if (v == -1)  AmAvcIdx -= 2;

			// Limit range
			if (AmAvcIdx < 12)
				AmAvcIdx = 90;
			else if (AmAvcIdx > 90)
				AmAvcIdx = 12;

			// Select
			currentAVC = AmAvcIdx;
		}

		// Configure SI4732/5
		rx.setAvcAmMaxGain(currentAVC);

		// Only call showAvc() if incr/decr action (allows the doAvc(0) to act as getAvc)
		//if (v != 0) showAvc();
		delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
	}
}

// -----------------------------------------------------------------------------------

void setup() {

	encoder.setBoundaries(-1, 1, false);
	encoder.begin();

	ui.lcd()->drawXBM(0, 0, welcome_width, welcome_height, welcome_bits);
	ui.updateDisplay();

	Wire.begin(ESP32_I2C_SDA, ESP32_I2C_SCL);

	delay(300);

	// EEPROM
	// Note: Use EEPROM.begin(EEPROM_SIZE) before use and EEPROM.begin.end after use to free up memory and avoid memory leaks
	EEPROM.begin(EEPROM_SIZE);

	// Press and hold Encoder button to force an EEPROM reset
	// Indirectly forces the reset by setting app_id = 0 (Detectected in the subsequent check for app_id and app_ver)
	// Note: EEPROM reset is recommended after firmware updates

	if (digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW)
	{
		EEPROM.write(eeprom_address, 0);
		EEPROM.commit();
		ui.showStatusScreen("EEPROM", "EEPROM Resetting...");
		ui.updateDisplay();
		delay(1000);
	}

	EEPROM.end();

	// Check for SI4732 connected on I2C interface
	// If the SI4732 is not detected, then halt with no further processing
	rx.setI2CFastModeCustom(100000);

	int16_t si4735Addr = rx.getDeviceI2CAddress(RESET_PIN); // Looks for the I2C bus address and set it.  Returns 0 if error

	if (si4735Addr == 0) {
		ui.showStatusScreen("ERROR", "RADIO hardware not found !");
		ui.updateDisplay();
		while (1);
	}

	rx.setup(RESET_PIN, MW_BAND_TYPE);

	cleanBfoRdsInfo();

	delay(300);

	// Perform check against app_id and app_ver
	uint8_t  id_read;
	uint16_t ver_read;

	EEPROM.begin(EEPROM_SIZE);
	id_read = EEPROM.read(eeprom_address);
	ver_read = EEPROM.read(eeprom_ver_address) << 8;
	ver_read |= EEPROM.read(eeprom_ver_address + 1);
	EEPROM.end();

	if ((id_read == app_id) && (ver_read == app_ver)) {
		readAllReceiverInformation();                        // Load EEPROM values
	}
	else {
		saveAllReceiverInformation();                        // Set EEPROM to defaults
		rx.setVolume(volume);                                // Set initial volume after EEPROM reset
	}

	// ** SI4732 STARTUP **
	// Uses values from EEPROM (Last stored or defaults after EEPROM reset) 
	useBand();

	sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
	//hal_fft_start();

}

// -----------------------------------------------------------------------------------

/**
 * Starts the MENU action process
 */
void doCurrentMenuCmd() {
	disableCommands();
	switch (currentMenuCmd) {
	case VOLUME:                   // VOLUME
		if (muted) {
			rx.setVolume(mute_vol_val);
			muted = false;
		}
		cmdVolume = true;
		break;
	case STEP:                      // STEP
		cmdStep = true;
		if (currentMode == FM) {
			ui.setMenu(currentStepIdx, FmStepStr, 4);
		}
		else if (isSSB()) {
			ui.setMenu(currentStepIdx, AmSsbStepStr, 8);
		}
		else {
			ui.setMenu(currentStepIdx, AmStepStr, 7);
		}
		break;
	case MODE:                      // MODE
		cmdMode = true;
		ui.setMenu(currentMode, modeStr, 5);
		break;
	case BW:                        // BW
		cmdBandwidth = true;
		if (currentMode == FM) {
			ui.setMenu(bwIdxFM, bandwidthFMStr, 5);
		}
		else if (isSSB()) {
			ui.setMenu(bwIdxSSB, bandwidthSSBStr, 6);
		}
		else {
			ui.setMenu(bwIdxAM, bandwidthAMStr, 7);
		}
		break;
	case AGC_ATT:                   // AGC/ATT
		cmdAgc = true;
		break;
	case SOFTMUTE:                  // SOFTMUTE
		if (currentMode != FM) {
			cmdSoftMuteMaxAtt = true;
		}
		else {
			showInfoMsg("Not available in FM mode !");
		}
		break;
	case SEEKUP:                    // SEEKUP
		seekStop = false;             // G8PTN: Flag is set by rotary encoder and cleared on seek entry
		seekDirection = 1;
		doSeek();
		break;
	case SEEKDOWN:                  // SEEKDOWN
		seekStop = false;             // G8PTN: Flag is set by rotary encoder and cleared on seek entry
		seekDirection = 0;
		doSeek();
		break;
	case BAND:                      // BAND
		cmdBand = true;
		ui.setMenu(bandIdx, bandStr, 8);
		break;
	case MUTE:                      // MUTE
		muted = !muted;
		if (muted)
		{
			mute_vol_val = rx.getVolume();
			rx.setVolume(0);
		}
		else rx.setVolume(mute_vol_val);
		break;

		// G8PTN: Added
	case CALIBRATION:               // CALIBRATION
		if (isSSB()) {
			cmdCal = true;
			currentCAL = band[bandIdx].bandCAL;
		}
		else {
			showInfoMsg("Only available in SSB mode !");
		}
		break;
		// G8PTN: Added
	case AVC:                       // AVC
		if (currentMode != FM) {
			cmdAvc = true;
		}
		else {
			showInfoMsg("Not available in FM mode !");
		}
		break;

	default:
		break;
	}
	currentMenuCmd = -1;
	elapsedCommand = millis();
}

void doEncoderAction() {
	// G8PTN: The manual BFO adjusment is not required with the doFrequencyTuneSSB method, but leave for debug
	if (bfoOn & isSSB())
	{
		currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
		// G8PTN: Clamp range to +/- BFOMax (as per doFrequencyTuneSSB)
		if (currentBFO > BFOMax) currentBFO = BFOMax;
		if (currentBFO < -BFOMax) currentBFO = -BFOMax;
		band[bandIdx].currentFreq = currentFrequency + (currentBFO / 1000);     // G8PTN; Calculate frequency value to store in EEPROM    
		updateBFO();
	}
	else if (cmdMenu)
		doMenu(encoderCount);
	else if (cmdMode)
		doMode(encoderCount);
	else if (cmdStep)
		doStep(encoderCount);
	else if (cmdAgc)
		doAgc(encoderCount);
	else if (cmdBandwidth)
		doBandwidth(encoderCount);
	else if (cmdVolume)
		doVolume(encoderCount);
	else if (cmdSoftMuteMaxAtt)
		doSoftMute(encoderCount);
	else if (cmdBand)
		setBand(encoderCount);

	// G8PTN: Added commands
	else if (cmdCal)
		doCal(encoderCount);
	else if (cmdAvc)
		doAvc(encoderCount);

	// G8PTN: Added SSB tuning
	else if (isSSB()) {

		doFrequencyTuneSSB();
		currentFrequency = rx.getFrequency();
	}
	else {

		// G8PTN: Used in place of rx.frequencyUp() and rx.frequencyDown()
		if (currentMode == FM)
			currentFrequency += tabFmStep[currentStepIdx] * encoderCount;       // FM Up/Down
		else
			currentFrequency += tabAmStep[currentStepIdx] * encoderCount;       // AM Up/Down

		// Band limit checking
		uint16_t bMin = band[bandIdx].minimumFreq;                            // Assign lower band limit
		uint16_t bMax = band[bandIdx].maximumFreq;                            // Assign upper band limit

		// Special case to cover AM frequency negative!
		bool AmFreqNeg = false;
		if ((currentMode == AM) && (currentFrequency & 0x8000))
			AmFreqNeg = true;

		// Priority to minimum check to cover AM frequency negative
		if ((currentFrequency < bMin) || AmFreqNeg)
			currentFrequency = bMax;                                           // Lower band limit or AM frequency negative
		else if (currentFrequency > bMax)
			currentFrequency = bMin;                                           // Upper band limit

		rx.setFrequency(currentFrequency);                                   // Set new frequency

		if (currentMode == FM) cleanBfoRdsInfo();
		// Show the current frequency only if it has changed
		currentFrequency = rx.getFrequency();
		band[bandIdx].currentFreq = currentFrequency;            // G8PTN: Added to ensure update of currentFreq in table for AM/FM
	}

	encoderCount = 0;
	resetEepromDelay();
	delay(MIN_ELAPSED_TIME);
	elapsedCommand = millis();
}

void doButtonAction() {
	//while (digitalRead(ENCODER_PUSH_BUTTON) == LOW) { }
	countClick++;
	if (cmdMenu)
	{
		currentMenuCmd = menuIdx;
		doCurrentMenuCmd();
	}
	//else if (countClick == 1)
	else if (countClick >= 1)                   // G8PTN: All actions now done on single press
	{
		if (isMenuMode())
		{
			disableCommands();
		}
		else if (bfoOn) {
			bfoOn = false;
		}
		else
		{
			cmdMenu = !cmdMenu;
			menuIdx = VOLUME;
			currentMenuCmd = menuIdx;
			ui.setMenu(menuIdx, MenuStr, 8);
		}
	}
	else                                       // G8PTN: Not used
	{ // GO to MENU if more than one click in less than 1/2 seconds.
		cmdMenu = !cmdMenu;
	}
	delay(MIN_ELAPSED_TIME);
	elapsedCommand = millis();
}


void drawMenu() {
	int menuSize = 208;
	if (isMenuMode()) {
		if (cmdMenu) {
			showMenuScreen("Menu", menuSize);
			ui.drawMenu();
		}
		else if (cmdMode) {
			showMenuScreen("Mode", 140);
			ui.drawMenu();
		}
		else if (cmdStep) {
			if (currentMode == FM) {
				menuSize = 120;
			}
			else if (isSSB()) {
				menuSize = 211;
			}
			else {
				menuSize = 185;
			}
			showMenuScreen("Step", menuSize);
			ui.drawMenu();
		}
		else if (cmdAgc) {
			showMenuScreen("AGC/ATTN", 100);
			if (agcNdx == 0 && agcIdx == 0) {
				ui.setFont(Font::FONT_B32_TF);
				ui.drawStringf(TextAlign::CENTER, 206, 382, 100, true, false, false, "ON");
			}
			else {
				ui.setFont(Font::FONT_56_NF);
				ui.drawStringf(TextAlign::CENTER, 206, 382, 110, true, false, false, "%2.2d", agcNdx);
			}
		}
		else if (cmdBandwidth) {
			if (currentMode == FM) {
				menuSize = 140;
			}
			else if (isSSB()) {
				menuSize = 155;
			}
			else {
				menuSize = 185;
			}
			showMenuScreen("Bandwidth", menuSize);
			ui.drawMenu();
		}
		else if (cmdVolume) {
			showMenuScreen("Volume", 100);
			ui.draw_ic24_sound_on(220, 75, BLACK);
			ui.setFont(Font::FONT_56_NF);
			ui.drawStringf(TextAlign::CENTER, 206, 382, 100, true, false, false, "%u", map(rx.getVolume(), 0, 63, 0, 100));
		}
		else if (cmdSoftMuteMaxAtt) {
			showMenuScreen("SoftMute", 100);
			ui.setFont(Font::FONT_56_NF);
			ui.drawStringf(TextAlign::CENTER, 206, 382, 100, true, false, false, "%2.2d", softMuteMaxAttIdx);
		}
		else if (cmdBand) {
			showMenuScreen("Band", menuSize);
			ui.drawMenu();
		}
		else if (cmdCal) {
			showMenuScreen("Calibration", 100);
			ui.setFont(Font::FONT_56_NF);
			ui.drawStringf(TextAlign::CENTER, 206, 382, 100, true, false, false, "%4.4d", currentCAL);
		}
		else if (cmdAvc) {
			showMenuScreen("AVC", 100);
			ui.setFont(Font::FONT_56_NF);
			ui.drawStringf(TextAlign::CENTER, 206, 382, 100, true, false, false, "%2.2d", currentAVC);

		}
	}
}

void loop() {
	// Check if the encoder has moved.

	pushButton.poll();

	if (encoder.encoderChanged()) {
		encoderCount = encoder.getEncoderValue();
		encoder.setEncoderValue(0);
	}

	if (encoderCount != 0)
	{
		infoShow = false;
		doEncoderAction();
	}
	else
	{
		// G8PTN: Modified to use new button detection. Disable band menu on single push. Default to volume option
		//if (digitalRead(ENCODER_PUSH_BUTTON) == LOW)
		if (pushButton.pushed())
		{
			infoShow = false;
			doButtonAction();
		}
	}

	// Show RSSI status only if this condition has changed
	if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 2)
	{

		rx.getCurrentReceivedSignalQuality();
		snr = rx.getCurrentSNR();
		int aux = rx.getCurrentRSSI();

		//if (rssi != aux && !isMenuMode())
		if (rssi != aux)                            // G8PTN: Based on 1.2s update, always allow S-Meter
		{
			rssi = aux;
		}
		elapsedRSSI = millis();
	}

	// Disable commands control
	if ((millis() - elapsedCommand) > ELAPSED_COMMAND)
	{
		if (isSSB())
		{
			bfoOn = false;
			disableCommands();
		}
		else if (isMenuMode()) {
			disableCommands();
		}
		if (infoShow) {
			infoShow = false;
		}

		elapsedCommand = millis();
	}

	if ((millis() - elapsedClick) > ELAPSED_CLICK)
	{
		countClick = 0;
		elapsedClick = millis();
	}

	if ((millis() - lastRDSCheck) > RDS_CHECK_TIME) {
		if ((currentMode == FM) and (snr >= 12)) checkRDS();
		lastRDSCheck = millis();
	}

	// Show the current frequency only if it has changed
	if (itIsTimeToSave)
	{
		if ((millis() - storeTime) > STORE_TIME)
		{
			saveAllReceiverInformation();
			storeTime = millis();
			itIsTimeToSave = false;
		}
	}

	// Periodically refresh the main screen
	// This covers the case where there is nothing else triggering a refresh
	if ((millis() - background_timer) > BACKGROUND_REFRESH_TIME) {
		background_timer = millis();
		showStatus();
		drawMainVFO();
		drawSpectrum();

		drawMenu();
		if (infoShow) {
			ui.showStatusScreen("", infoMessage);
		}
		ui.updateDisplay();
	}

	// Add a small default delay in the main loop
	//delay(5);


}
