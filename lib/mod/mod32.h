#pragma once

/*
   Original code by  "Pascal Piazzalunga" - http://www.serveurperso.com
   Adapted for bitbox by makapuf - makapuf2@gmail.com
 */


#include <stdbool.h>
#include <stdint.h>
#include "bitbox.h"

#define MOD_ENABLE_LINEAR_INTERPOLATION

#define DIVIDER 10                      // Fixed-point mantissa used for integer arithmetic

#define MOD_STEREOSEPARATION 64                       // 0 (max) to 64 (mono) 

// Hz = 7093789 / (amigaPeriod * 2) for PAL
// Hz = 7159091 / (amigaPeriod * 2) for NTSC
#define AMIGA (7093789 / 2 / BITBOX_SAMPLERATE << DIVIDER)
// Mixer.channelFrequency[channel] = AMIGA / amigaPeriod

#define ROWS 64
#define SAMPLES 31

#ifndef MOD_CHANNELS
#define MOD_CHANNELS 4 // defaults to 4
#endif 

#define NONOTE 0xFFFF


struct Sample {
    uint8_t name[22];
    uint16_t length;
    int8_t fineTune;
    uint8_t volume;
    uint16_t loopBegin;
    uint16_t loopLength;
};

struct Mod {
    uint8_t name[20];
    struct Sample samples[SAMPLES];
    uint8_t songLength;
    uint8_t _dummy;
    uint8_t order[128];
    char tag[4];
};

struct Pattern {
    uint8_t sampleNumber[ROWS][MOD_CHANNELS];
    uint16_t note[ROWS][MOD_CHANNELS];
    uint8_t effectNumber[ROWS][MOD_CHANNELS];
    uint8_t effectParameter[ROWS][MOD_CHANNELS];
};

struct Player {
	// all precomputed
    struct Pattern currentPattern;

    uint8_t numberOfPatterns; 
    uint8_t numberOfChannels; 

    uint32_t amiga;
    uint16_t samplesPerTick;
    uint8_t speed;
    uint8_t tick;
    uint8_t row;
    uint8_t lastRow;

    uint8_t orderIndex;
    uint8_t oldOrderIndex;
    uint8_t patternDelay;
    uint8_t patternLoopCount[MOD_CHANNELS];
    uint8_t patternLoopRow[MOD_CHANNELS];

    uint8_t lastSampleNumber[MOD_CHANNELS];
    int8_t volume[MOD_CHANNELS];
    uint16_t lastNote[MOD_CHANNELS];
    uint16_t amigaPeriod[MOD_CHANNELS];
    int16_t lastAmigaPeriod[MOD_CHANNELS];

    uint16_t portamentoNote[MOD_CHANNELS];
    uint8_t portamentoSpeed[MOD_CHANNELS];

    uint8_t waveControl[MOD_CHANNELS];

    uint8_t vibratoSpeed[MOD_CHANNELS];
    uint8_t vibratoDepth[MOD_CHANNELS];
    int8_t vibratoPos[MOD_CHANNELS];

    uint8_t tremoloSpeed[MOD_CHANNELS];
    uint8_t tremoloDepth[MOD_CHANNELS];
    int8_t tremoloPos[MOD_CHANNELS];
};

struct Mixer {
    uint32_t sampleBegin[SAMPLES];
    uint32_t sampleEnd[SAMPLES];
    uint32_t sampleloopBegin[SAMPLES];
    uint16_t sampleLoopLength[SAMPLES];
    uint32_t sampleLoopEnd[SAMPLES];

    uint8_t channelSampleNumber[MOD_CHANNELS];
    uint32_t channelSampleOffset[MOD_CHANNELS];
    uint16_t channelFrequency[MOD_CHANNELS];
    uint8_t channelVolume[MOD_CHANNELS];
    uint8_t channelPanning[MOD_CHANNELS];
};

// External prototype
// load and play song. use NULL to stop.
// this is the only function to call
void load_mod(const void* modfile); 

// defined but called internally by game_snd_buffer. no need to use it for standard uses.
extern struct Player mod32_player;
extern struct Mod *mod32_mod;
extern struct Mixer mod32_mixer;
void update_player(void);
uint16_t gen_sample (uint16_t *buffer);
