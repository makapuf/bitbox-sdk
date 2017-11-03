This a chiptune library by PulkoMandy.
See the correspnding tracker at https://github.com/pulkomandy/bitbox-chiptracker

You can also see a simple example on bitbox main repo which basically consists of : 

	#include "bitbox.h"
	#include "lib/chiptune/player.h"

	extern const struct ChipSong SONG;

	void bitbox_main() {
		chip_play(&SONG);
	}
	// ouch, that was hard !
