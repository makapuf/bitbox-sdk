#pragma once
/*
todo : add to_delete as a magic number
introduce utlist +3 lists heads : wait_list, draw_list, past_list
use utlist sort ?
subobjects ?
py3 / clean utils
iface c++
sprite_set2x(tf), btc_set2x()
lock
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
	int16_t z;

	// live data (typically in RAM, stable per frame)
	int16_t x,y; // ry is real Y, while y is wanted y, which will be activated next frame.
	uint16_t w,h,fr; // object size, frame is frame id

	uintptr_t a,b,c,d; // various 32b used for each blitter as extra parameters or internally

	// inline single linked lists (engine internal)
	struct object *next;
} object;


void blitter_insert(object *o, int16_t x, int16_t y, int16_t z); // insert to display list
void blitter_remove(object *o);

void blitter_frame(void); // callback for frames.
void blitter_line(void);

// creates a new object, activate it, copy from object.
void rect_init(struct object *o, uint16_t w, uint16_t h, pixel_t color);

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
