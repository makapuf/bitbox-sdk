// test simple by default
#include <string.h>

#include "simple.h"
#include "sampler.h"
#include "bitbox.h" // gamepad

const char *instrs[5] = {
  "This is a demonstration of Bitbox simple sampler with sound in flash ",
  "(opposed to sounds streamed from sdcard)",
  "press ABXY / UDLR buttons on the gamepad (or the emulated version on the",
  "keyboard) and play with the sounds (sound output needs to be plugged ",
  "of course). Check the source to see how to integrate sounds in your games"
};

extern const int8_t snd_diam_raw[], 
    snd_bitbox_rules_raw[], 
    snd_pouet_raw[], 
    snd_guitar_raw[],
    snd_piano_raw[],
    snd_sax_raw[], 
    snd_monster_raw[], 
    snd_warplevel_raw[];

extern const unsigned int snd_diam_raw_len, 
    snd_bitbox_rules_raw_len, 
    snd_pouet_raw_len, 
    snd_guitar_raw_len,
    snd_piano_raw_len,
    snd_sax_raw_len, 
    snd_monster_raw_len, 
    snd_warplevel_raw_len;


void game_init() {
    // draw text
    for (int i=0;i<5;i++)
        strcpy(&vram[(2+i)*SCREEN_W+4],instrs[i]);
    
    audio_on=1;

}

#define ifplay(but,sndname) \
    if((gamepad_buttons[0] & ~last_buts) & gamepad_##but) \
        play_mono_from_memory(snd_##sndname##_raw, snd_##sndname##_raw_len,40,40)

void game_frame() {
    static uint16_t last_buts;

    kbd_emulate_gamepad ();

    ifplay(A,bitbox_rules);
    ifplay(B,monster);
    ifplay(X,pouet);
    ifplay(Y,guitar);

    ifplay(up,sax);
    ifplay(down,piano);
    ifplay(left,warplevel);
    ifplay(right,diam);
       
    last_buts = gamepad_buttons[0];
}