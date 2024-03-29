
// --- 16x16 or 8x8 Tilemaps
// --------------------------------------------------------------------------------------

/*
    RAM data :

        *data : start of tilemap
        a : tileset
        b : header

    - width and height are displayed sizes, can be bigger/smaller than tilemap, in which case it will loop

    To initialize the tilemap object, you can also use 0 to mean "same size as tilemap"

    tilemap references can be u16, i16 (semi transparent tiles), u8 or i8 (not all options implemented now)

    header
            u12 : width of tilemap in tiles
            u12 : height in tiles
            u1 : RFU
            u1 : 0 : 16-bit color tileset 1:8-bit color tileset
            u2 : tilesize. 00 = 16x16, 01 = 32x32 tiles 10 = 8x8 
            u4 : tilemap_index_type = 0:u16, 1:u8, 2:i16, 3:i8

    void *data : tile_index either u8 or u16 ...

    tilemap index 0 are always transparent (ie no tile, so first tile in tileset has index 1)


 */
#include "blitter.h"
#include "string.h"

const int tilesizes[] = {16,32,8};
#define TMAP_HEADER(w,h,tilesizecode,tmaptype) (w<<20 | h<<8 | (tilesizecode)<<4 | (tmaptype)) // w:12 h:12 sizecode:4 maptype:4
#define TMAP_WIDTH(header) (header>>20)


// tiles width 
#define TSET_16 0
#define TSET_32 1
#define TSET_8 2
// index type
#define TMAP_U8 1
#define TMAP_U16 2


#define min(a,b) (a<b?a:b)

/* COPY8 ? __asm__ (
    "ldmia %[src]!,{r0-r7} \r\n"
    "stmia %[dst]!,{r0-r7}"
    :[src] "+r" (src), [dst] "+r" (dst)
    :: "r0","r1","r2","r3","r4","r5","r6","r7"
    );
*/

__attribute__((always_inline)) static inline void tilemap_u8_line8(object *o, const unsigned int tilesize) 
{
    // use current frame, line, buffer
    unsigned int tilemap_w = o->b>>20;
    unsigned int tilemap_h = (o->b >>8) & 0xfff;

    //o->x &= ~3; // force addresses mod4

    // --- line related
    // line inside tilemap (pixel), looped.
    int sprline = (vga_line-o->y) % (tilemap_h*tilesize);
    // offset from start of tile (in lines)
    int offset = sprline%tilesize;
    // pointer to the beginning of the tilemap line
    uint8_t *idxptr = (uint8_t *)o->data+(sprline/tilesize) * tilemap_w; // all is in nb of tiles

    // --- column related -> in frame ?
    // horizontal tile offset in tilemap, draw position
    int tile_x, ofs;
    if (o->x >= 0 ) {
        tile_x = 0;
        ofs = o->x;
    } else {
        tile_x = (-o->x/tilesize) % tilemap_w;
        ofs = o->x%(int)tilesize;
    }
    
    uint8_t * restrict dst = (uint8_t*) &draw_buffer[ofs];
    // pixel addr of the last pixel
    const uint8_t *dst_max = (uint8_t*) &draw_buffer[min(o->x+o->w, VGA_H_PIXELS)];

    uint8_t *tiledata = (uint8_t *)o->a; // nope : read 4 indices at once.
    uint8_t *restrict src;  // __builtin_assume_aligned

    // blit to end of line (and maybe a little more)
    // we needed to loop over

    while (dst<dst_max) {
        if (tile_x>=tilemap_w)
            tile_x-=tilemap_w;

        // blit one tile, 2pix=32bits at a time, 8 times = 16pixels, 16 times=32pixels
        if (idxptr[tile_x]) {
            src = &tiledata[(idxptr[tile_x]*tilesize + offset)*tilesize];

            // Verify out of the loop, fix tilesize ?
            // assume aligned.
            memcpy(dst,src,tilesize);

        };
        dst += tilesize; // words per tile
        tile_x++;
    }
}



// specialize - generic case
void tilemap_u8_line8_any(object *o) {
    tilemap_u8_line8(o, tilesizes[((o->b)>>4)&3]);
}

// specialize - 8 pixels wide case
void tilemap_u8_line8_8(object *o) {
    tilemap_u8_line8(o, 8);
}

void tilemap_init (struct object *o, const struct TilesetFile *tileset, int map_w, int map_h, const void *tilemap) {
     o->data = (uint32_t *)tilemap;

    if (tileset->nbtiles > 256) {
        message("only 8bit tilemap indices handled for now\n");
        bitbox_die(4,5);        
    }
    if (tileset->datacode != 1) {
        message("only 8bit tilesets can be blit on 8bpp displays\n");
        bitbox_die(4,7);
    }


    o->b = TMAP_HEADER(map_w,map_h,tileset->tilesize == 8 ? TSET_8 : TSET_16, TMAP_U8); // only 8bits tilemap indices for now.

    // generic attributes
    // 0 for object width == tilemap width
    o->w=map_w*tileset->tilesize;
    o->h=map_h*tileset->tilesize;
    // layer
    o->fr = 0;

    o->frame=0;

    o->a = ((uintptr_t)(tileset->data))-tileset->tilesize*tileset->tilesize; // to start at index 1 and not 0, offset now in bytes.

    o->line = tileset->tilesize == 8 ? tilemap_u8_line8_8 : tilemap_u8_line8_any;    
}


void tilemap_init_file (struct object *o, const struct TilesetFile *tileset, const struct TilemapFile *tilemap) {
   if (tilemap->codec != 1) {
        message("only 8bit tilemap indices can be used now\n");
        bitbox_die(4,5);
    }
    tilemap_init(o, tileset, tilemap->map_w, tilemap->map_h, tilemap->data);
}

// blit a tilemap file to x,y position to tilemap vram.
// will NOT update or check tileset 
void tmap_blit_file(object *tm, int x, int y, const struct TilemapFile *tf, const unsigned layer) {

    uint32_t dst_header = tm->b;
    int dst_w = (dst_header>>20);
    int dst_h = ((dst_header>>8) & 0xfff);
    int dst_type = dst_header & 0x0f;

    if (dst_type != tf->codec) {
        message ("Error blitting tmap : src type : %d, dst type %d\n", tf->codec, dst_type);
        bitbox_die(5,5);
    }

    uint8_t *src_data = tmap_layer_ofs(tf,layer);

    for (int j=0;j<tf->map_h && j<(dst_h-y);j++) {
        // memcpy ... 
        for (int i=0;i<tf->map_w && i<(dst_w-x);i++) {
            if ((dst_type & 0x0f )==TMAP_U8)  { // only consider tmap type
                uint8_t c = ((uint8_t*)src_data) [tf->map_w * j+i ];
                if (c) ((uint8_t *)tm->data) [(j+y)*dst_w+i+x] = c;
            } else {
                uint16_t c = ((uint16_t*)src_data)[tf->map_w * j+i ];
                if (c) ((uint16_t *)tm->data)[(j+y)*dst_w+i+x] = c;
            }
        }
    }
}

void *tmap_layer_ofs (const struct TilemapFile *tmap_file, const unsigned n) {
    if (n >= tmap_file->nb_layers) {
        message ("Error blitting tmap : layer %d does not exist\n", n);
        bitbox_die(5,1);
    }
    // index in bytes
    const unsigned int ofs = tmap_file->map_w * tmap_file->map_h * n * (tmap_file->codec == 0 ? 2 : 1);

    return (void *)&tmap_file->data[ofs/sizeof(tmap_file->data[0])];
}

const struct TilemapFileObject *tmap_objects(const struct TilemapFile *tmap_file) {
    return (const struct TilemapFileObject *) tmap_layer_ofs(tmap_file, tmap_file->nb_layers);
}
