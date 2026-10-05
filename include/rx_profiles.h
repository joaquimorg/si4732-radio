#pragma once
#include <Arduino.h>
#include "global.h"

/*
 * Receiver profiles (menu "RX Mode")
 * ==================================
 *
 * A profile is only a starting point: applying it writes the normal per mode settings (AGC/ATTN, AVC,
 * SoftMute, bandwidth), which the user can then fine tune in their own menus. The main menu shows the
 * profile that matches the current settings, or "Custom".
 *
 * How the SI4732 gain chain works (AN332), which is why these values were chosen:
 *
 *  - AGC (RF/IF gain, front end). Enabled, the chip lowers the front end gain only as much as the
 *    strongest signal inside its wideband detector requires. That is the best setting for weak signals,
 *    so every profile keeps AGC ON with no attenuation. With AGC off, AGCIDX 0 is the MAXIMUM gain and
 *    1..36 add attenuation: use it by hand (menu AGC/ATTN) only when strong signals overload the receiver
 *    (intermodulation products, "splatter" all over the band, stations that appear where none should).
 *    Attenuation reduces signal and the noise of the first stages alike, so with a strong noise floor
 *    from the antenna it may cost nothing; on a quiet band it costs weak signals.
 *
 *  - AVC (AM_AUTOMATIC_VOLUME_CONTROL_MAX_GAIN, 12..90 dB). This is AUDIO gain after demodulation: the
 *    maximum gain the volume control may add when the signal is weak. It does NOT improve sensitivity
 *    or SNR. A higher value makes weak signals louder, but it makes band noise between words and during
 *    fades just as loud ("hiss pumping"). Lower values keep the noise down at the price of weak signals
 *    being quieter. The SSB patch uses the same property.
 *
 *  - SoftMute (0..32 dB). Attenuates the audio when the SNR drops below the threshold. It makes tuning
 *    between stations quieter, but it attenuates exactly the weak signals a DX listener wants, and it
 *    makes fading SSB/CW signals pump. Hence 0 (off) for SSB/CW and a few dB at most for AM.
 *
 *  - Bandwidth. The narrowest filter that still passes the signal gives the best SNR: noise power is
 *    proportional to bandwidth. SSB voice needs about 300..2400 Hz; AM needs twice the audio bandwidth.
 *    The SSB 0.5 / 1.0 / 1.2 kHz filters use the sideband band-pass filter (SBCUTFLT = 0); 2.2 kHz and
 *    wider use the low-pass filter (SBCUTFLT = 1), as AN332 recommends (see rxWriteSsbMode() in main.cpp).
 *
 * The values are educated starting points, not measured optimums: compare them on the air
 * (same signal, same antenna, switching profiles) and change them here.
 */

#define RXP_ANY  0xFF                       // Keeps the current mode
#define RXP_SSB  0xFE                       // LSB or USB (keeps the current one, else LSB below 10 MHz)

struct RxProfile {
	const char* name;                       // Menu text
	uint8_t mode;                           // AM, CW, RXP_SSB or RXP_ANY
	bool agcEnabled;                        // AGC on. Every profile keeps it on, see above
	uint8_t attenuation;                    // AGC index (0 = max gain, 1..36) when agcEnabled is false
	uint8_t avcMaxGain;                     // dB, one of rxAvcSteps[]
	uint8_t softMuteAM;                     // dB, used in AM
	uint8_t softMuteSSB;                    // dB, used in LSB/USB/CW
	uint8_t bwAM;                           // bandwidthAM[] position:  0=1.0 1=1.8 2=2.0 3=2.5 4=3.0 5=4.0 6=6.0 kHz
	uint8_t bwSSB;                          // bandwidthSSB[] position: 0=0.5 1=1.0 2=1.2 3=2.2 4=3.0 5=4.0 kHz
};

const RxProfile rxProfiles[] = {
	// NORMAL: general listening. Moderate audio gain, a little AM SoftMute to quiet the noise between
	// stations, 3 kHz filters (the chip has 2.2 or 3.0 kHz for SSB; 3.0 sounds natural on good signals).
	{ "Normal", RXP_ANY,  true, 0,  36, 4, 0,   4 /* 3.0 */, 4 /* 3.0 */ },

	// DX: weak signals in the current mode. More audio gain so weak signals are audible, no SoftMute so
	// they are not attenuated, narrow filters for the best SNR and adjacent channel rejection.
	{ "DX",     RXP_ANY,  true, 0,  48, 0, 0,   2 /* 2.0 */, 3 /* 2.2 */ },

	// LOCAL: strong signals. Less audio gain keeps the noise down, SoftMute on, wide filters for audio
	// quality. If strong stations still overload the receiver, add attenuation in AGC/ATTN.
	{ "Local",  RXP_ANY,  true, 0,  24, 8, 4,   6 /* 6.0 */, 4 /* 3.0 */ },

	// AM_DX: weak AM broadcast. 2.5 kHz has a gradual roll-off: almost the selectivity of 2.0 kHz with
	// more intelligible audio. AVC 42 is between Normal and DX; 2 dB of SoftMute keeps the noise of deep
	// fades down without muting the signal.
	{ "AM DX",  AM,       true, 0,  42, 2, 0,   3 /* 2.5 */, 4 /* 3.0 */ },

	// SSB_DX: weak SSB voice. 2.2 kHz passes the speech band that carries intelligibility and rejects the
	// rest. AVC 36: more gain mostly amplifies noise in SSB. SoftMute off: it pumps on fading SSB.
	{ "SSB DX", RXP_SSB,  true, 0,  36, 0, 0,   2 /* 2.0 */, 3 /* 2.2 */ },

	// CW_DX: weak CW. 500 Hz band-pass around the CW_TONE_HZ beat note. AVC 30: the narrow filter already
	// passes little noise, but a lower AVC keeps the noise floor from being amplified between characters.
	{ "CW DX",  CW,       true, 0,  30, 0, 0,   2 /* 2.0 */, 0 /* 0.5 */ },
};
const uint8_t rxProfileCount = sizeof(rxProfiles) / sizeof(rxProfiles[0]);

// AVC maximum gain steps (dB) offered by the AVC menu. The SI4732 accepts 12..90 dB; these steps are far
// enough apart to hear the difference. Values stored before (any even value) still work: the next
// encoder step moves to the nearest step in the turning direction.
const uint8_t rxAvcSteps[] = { 12, 18, 24, 30, 36, 42, 48, 54, 60, 72, 84, 90 };
const uint8_t rxAvcStepCount = sizeof(rxAvcSteps) / sizeof(rxAvcSteps[0]);
