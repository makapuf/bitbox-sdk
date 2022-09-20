#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
subobjects ?
clean utils
btc_set2x()
lock ?
new tilemaps (multi)
C++
u16/u8 pixel_t ?
*/

#include <stdint.h>
#include <bitbox.h>

/* Blitter : tilemap engine for bitbox.

	To use it with a 8-bit interface, please define VGA_BPP=8
    don't modify an object or add, or remove objects during active video ! (ie if vga_line<VGA_H_PIXELS)

*/

typedef struct object
{
	// static data (typically in flash)
	void *data; // this will be the source data
	void (*frame)(struct object *o, int line);
	void (*line) (struct object *o);

	int16_t x,y,z;
	uint16_t w,h,fr; // object size, frame is frame id

	uintptr_t a,b,c,d; // various 32b used for each blitter as extra parameters or internally

	// inline single linked lists (engine internal)
	struct object *next;
} object;


void blitter_insert(object *o, int16_t x, int16_t y, int16_t z); // insert to display list
void blitter_remove(object *o);

// ---------------------------------------------------------------------------------------------------
// --- Rect

void rect_init(struct object *o, uint16_t w, uint16_t h, pixel_t color);

// ---------------------------------------------------------------------------------------------------
// --- Sprites

// file format of a .spr file
struct SpriteFileHeader {
    uint16_t magic;
    uint16_t width;
    uint16_t height;
    uint8_t frames;
    uint8_t datacode;
    uint16_t x1,y1,x2,y2; // hitbox
    uint16_t data[]; // frame_start indices, then len+couple_palette[len] if couples, then data
};
void sprite3_load (struct object *o, const void *sprite_data);
static inline uint8_t sprite3_nbframes(const object *o) { return ((uint8_t *)o->a)[6]; }

void sprite3_toggle2X(object *o); // toggle between standard and 2X mode
void sprite3_set_solid(object *o, pixel_t color); // set solid color or 0 to reset

// ---------------------------------------------------------------------------------------------------
// --- Videos

void btc4_init    (struct object *o, const uint32_t *btc);
void btc4_2x_init (struct object *o, const uint32_t *btc);

// ---------------------------------------------------------------------------------------------------
// --- tilemaps

// file format of tile maps, can be in flash
struct TilemapFileObject {
	int16_t x,y;
	uint8_t type;  // if 255, next object group
	uint8_t v[3]; // if 255 and oid==255, end
};

struct TilemapFile {
    uint16_t magic; // 0xb17b
    uint16_t map_w,map_h;
    uint8_t nb_layers; // always blit last layer on screen
    uint8_t codec; // 0:u16 1:u8

    uint16_t data[]; // nb_layers*map_w*map_h of 2b tileset + 14b tileindex
    // at the end : tmapfileobjects
};

struct TilesetFile {
	uint8_t  tilesize; // 8 or 16
    uint8_t  datacode; // 0 = u16, 1=u8, 2=couples references in palette
    uint16_t nbtiles;
    uint16_t data[]; // (optional) : couples palette as u16, then u8 or u16 data[]
};

// initialize a tilemap from a tileset and some tilemap data. u8 indices for now
void tilemap_init (struct object *o, const struct TilesetFile *tileset, int map_w,int map_h, const void *tilemap);

// pointer to data for layer N. data is u16 but the real data could be u8 !
void *tmap_layer_ofs (const struct TilemapFile *tmap_file, const unsigned n);

// get pointer to first object, can iterate from there
const struct TilemapFileObject *tmap_objects(const struct TilemapFile *tmap_file);

// copy a file layer to object tilemap. will not change tileset
void tmap_blit_file(object *tm, int x, int y, const struct TilemapFile *tf, const unsigned layer);

// ---------------------------------------------------------------------------------------------------
// --- surfaces : 2bpp fast-blit elements

// layout : data = buffer : 4x4 couple palettes + data as 2bpp pixels. a,b,c,d : not used.

#define SURFACE_BUFSZ(w,h) ( (w+15)/16*4*h + 16*2*sizeof(pixel_t) )

void surface_init (struct object *o, int _w, int _h, void *_data); // width a multiple of 16
void surface_clear (struct object *o);
void surface_setpalette (struct object *o, const pixel_t *pal);
void surface_fillrect (struct object *o, int x1, int y1, int x2, int y2, uint8_t color);

// structure of font file (see mk_font)
struct Font {
	uint8_t height, bytes_per_line;
	uint8_t char_width [128]; // width in pixel of each character.
	uint8_t data[]; // 2bpp, integer number of bytes by line of character
};

// draw a single char, returns char width
int surface_char (struct object *o, const char c, int x, int y, const void *fontdata); 

// draw a string, including \n, \t characters
void surface_text (struct object *o, const char *text, int x, int y,const void *fontdata);


#ifdef __cplusplus
} // extern C
#endif
