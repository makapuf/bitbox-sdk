# Info
This is a .MOD player for bitbox !

This version runs mods embedded in binary, not relying on a fatfs implmentation.
Tip : to make several songs share the same set of instruments - to save space,
put all songs in the same mod file, loop the songs and start the song at a given position.

# Credits
 * Original code by  "Pascal Piazzalunga" - http://www.serveurperso.com
 * Bitbox port : makapuf

# Todo
- allow python exporter to massage the mod file to avoid parsing from memory, generate C structs directly and be able to share samples.
- allow single note playing for SFX (or inject whole minipatterns ?)
