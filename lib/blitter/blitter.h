#pragma once
/*
subobjects ?
clean utils
sprite_set2x(tf), btc_set2x()
lock ?
new tilemaps (multi)
C++
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

// creates a new object, activate it, copy from object.
void rect_init(struct object *o, uint16_t w, uint16_t h, pixel_t color);


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

void btc4_init    (struct object *o, const uint32_t *btc);
void btc4_2x_init (struct object *o, const uint32_t *btc);

#define TSET_16 0
#define TSET_32 1
#define TSET_8 2

#define TMAP_U8 1
#define TMAP_U16 2

#define TSET_8bit (1<<7)

#define TMAP_HEADER(w,h,tilesize,tmaptype) (w<<20 | h<<8 | (tilesize)<<4 | (tmaptype))
#define TMAP_WIDTH(header) (header>>20)

#define TILEMAP_6464u8 TMAP_HEADER(64,64,TSET_16, TMAP_U8)
#define TILEMAP_3232u8 TMAP_HEADER(32,32,TSET_16, TMAP_U8)
#define TILEMAP_6464u832 TMAP_HEADER(32,32,TSET_32,TMAP_U8)

void tilemap_init (struct object * o, const void *tileset, int w, int h, uint32_t header, const void *tilemap);
/*
	- tileset is a list of 16x16 u16 pixels. It will be 1-indexed by tilemap (or 32x32)
    - width and height are displayed sizes, can be bigger/smaller than tilemap, in which case it will loop
	tilemap size can be 32x32, 64x32, 64x32, (or any other, but need to be standard).

	To initialize the tilemap object, you can also use 0 to mean "same size as tilemap"

	tilemap references can be u16, i16 (semi transparent tiles), u8 or i8 (not all options implemented now)

	header
	        u12 : width of tilemap in tiles
	        u12 : height in tiles
	        u1 : RFU
	        u1 : 0 : 16-bit color tileset 1:8-bit color tileset
	        u2 : tilesize. 00 = 16x16, 01 = 32x32 tiles
	        u4 : tilemap_index_type = 0:u16, 1:u8, 2:i16, 3:i8

	void *data : tile_index either u8 or u16 ...

	tilemaps are created with a z=100 by default (to be behind sprites)

	tilemap index 0 are always transparent (ie no tile, so first tile in tileset has index 1)
*/

/*
	blits a tilemap into the object tilemap at x,y position (x,y in tiles)
 */
void tmap_blit(object *dst, int x, int y, uint32_t src_header, const void *src_data);

/* Blit a given layer of a tilemap to an object */
void tmap_blitlayer(object *tm, int x, int y, uint32_t src_header, const void* data, int layer);

struct TilemapFileObjects {
	int16_t x,y;
	uint8_t oid;
	uint8_t v[3];
};

struct TilemapFile {
    uint16_t magic; // 0xb17b
    uint16_t map_w,map_h;
    uint8_t nb_layers; // always blit last layer on screen
    uint8_t __rfu;

    uint16_t data[]; // nb_layers*map_w*map_h of 2b tileset + 14b tileindex
    // at the end : tmapfileobjects
};


// --- surfaces : 2bpp fast-blit elements
// layout : data = buffer : 4x4 couple palettes, data as 2bpp pixels. a,b,c,d : not used.

#define SURFACE_BUFSZ(w,h) ( (w+15)/16*4*h + 16*sizeof(couple_t) )

void surface_init (struct object *o, int _w, int _h, void *_data);
void surface_setpalette (struct object *o, const pixel_t *pal);
void surface_fillrect (struct object *o, int x1, int y1, int x2, int y2, uint8_t color);

// structure of font file (see mk_font)
struct Font {
	uint8_t height, bytes_per_line;
	uint8_t char_width [128]; // width in pixel of each character.
	uint8_t data[]; // 2bpp, integer number of bytes by line of character
};

int surface_chr (struct object *o, const char c, int x, int y, const void *fontdata); 
// draw a single char

void surface_text (struct object *o, const char *text, int x, int y,const void *fontdata);
// draw a string, including \n, \t characters
