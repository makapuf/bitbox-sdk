// External prototype
/*
   Original code by  "Pascal Piazzalunga" - http://www.serveurperso.com
   Adapted for bitbox by makapuf - makapuf2@gmail.com
 */

// load and play song. use modfile=NULL to stop.
void load_mod(const void* modfile);

// plays individual note on given track (used for sfx ...)
// Note : C4 is note 214, full volume is 64
void mod_play_note(uint8_t sample_id, uint8_t channel, uint8_t volume, uint8_t note); // volume 0-64

// jumps to given song position
void mod_jumpto(int order);
