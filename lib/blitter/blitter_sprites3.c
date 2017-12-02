// blitter_sprites3.c : simpler blitter objects. .
// All in 8bpp, define vga_palette symbol as uint16_t [256] to expand later

/* object layout in memory : 
 data : raw 8/16b data (start of blits)
 a : start of raw source data

 b : palette if couple palette
 d : if hi u16 not zero, replace with this color. if d&1 render with size = 2x (modify w/h as needed!)
 c : current blitting pos in data
 */

#include <string.h> // memcpy, memset
#include "blitter.h"

static void sprite3_frame(struct object *o, int line);
static void sprite3_line_noclip(struct object *o);
static void sprite3_line_clip(struct object *o);
static void sprite3_cpl_line_noclip (object *o);
// static void sprite3_line_flash (struct object *o);

#define DATACODE_u16 0
#define DATACODE_u8 1
#define DATACODE_c8 2

#define BLIT_SKIP 0
#define BLIT_FILL 1
#define BLIT_COPY 2
#define BLIT_BACK 3

#define MARGIN 32

#if VGA_BPP==8
typedef uint16_t couple_t;
#else
typedef uint32_t couple_t;
#endif 

struct SpriteHeader {
    uint16_t magic; 
    uint16_t width; 
    uint16_t height; 
    uint8_t frames; 
    uint8_t datacode; 
    uint16_t x1,y1,x2,y2; // hitbox
    uint32_t data[]; // frame_start indices, then len+couple_palette[len] if couples, then data 
};

struct object *sprite3_new(const void *data, int x, int y, int z)
{
	object *o = blitter_new();
    if (!o) return 0; // error : no room left

    const struct SpriteHeader *h=data;

    if (h->magic!=0xB17B) {
    	message("Error : wrong header found");
    	die(8,8);
    }

    o->w = h->width;
    o->h = h->height;
    
    o->a = (uintptr_t)h; 
    if (h->datacode == DATACODE_c8) {
        uint32_t palette_len = h->data[h->frames];
        o->b = (uintptr_t) &h->data[h->frames+1];
        o->data = (void *) &h->data[h->frames+1+palette_len];
    } else {
        o->b=0;
        o->data = (void*) &h->data[h->frames]; 
    }

    o->frame = sprite3_frame;

    // default values
    o->x=x;
    o->y=y;
    o->z=z;
    o->fr=0;

    return o;
}

// read length from src, pointing at a blit header.
static inline int read_len(uint8_t * restrict * src)
{
    int nb = (*(*src)++) & 31;
    if (nb==31) {
        uint8_t c;
        do {
            c = *(*src)++;
            nb += c;
        } while (c==255);
    }
    return nb;
}

static void sprite3_frame(struct object *o, int start_line)
{
    // start line is how much we need to crop to handle out of screen data

    o->c  = (intptr_t)o->data;

    // Skip to frame
    struct SpriteHeader *h = (struct SpriteHeader*)o->a;
    o->c += h->data[o->fr]; 
    start_line %= o->h;

    // Skip first lines as needed -> extract from noclip ? 
    while (start_line) {
        uint8_t h = *(uint8_t *)o->c; // blit header
        int nb = read_len((uint8_t **)&(o->c));
        if (h&1<<5) start_line--;
        switch(h>>6) {
            case BLIT_SKIP : 
                break; 
            case BLIT_COPY : 
                // o->b => a palette ? cpl refs
                o->c += o->b ? (nb+1)/2 : nb*sizeof(pixel_t); 
                break;
            case BLIT_BACK : 
                o->c += 2; 
                break; 
            case BLIT_FILL : 
                o->c += o->b ? 1 : sizeof(pixel_t);
                break;
        }
    }

    // select if clip or noclip (choice made each line)
    if (o->b) {
        o->line = sprite3_cpl_line_noclip;
        // fixme clipping 
    } else {    
        if ( o->x < -MARGIN || o->x - (int)o->w >= VGA_H_PIXELS+MARGIN )
            o->line = sprite3_line_clip;
        else 
            o->line = sprite3_line_noclip;
    }

    //o->line = sprite3_line_debug;

}

static void sprite3_line_noclip (struct object *o)
{
    uint8_t * restrict src = (uint8_t *)o->c; 
    pixel_t * restrict dst = &draw_buffer[o->x]; 
    uint8_t header;
    uint16_t backref;

    do {
        header = *src; 

        int nb = read_len(&src);

        switch (header >> 6) {
            case BLIT_SKIP:
                dst += nb;
                break;
            case BLIT_COPY: 
                memcpy(dst,src,nb*sizeof(pixel_t));
                dst+=nb;
                src+=nb*sizeof(pixel_t);
                break;
            case BLIT_BACK : // back reference as u16
                backref = src[0] << 8 | src[1];
/*
                message(
                    "at %d print backref sz %d ref %d\n",
                    src-(uint8_t*)o->data,nb,backref
                    );
*/
                memcpy(dst,src-backref,nb*sizeof(pixel_t));
                src += 2; // always u16
                dst += nb;
                break;
            case BLIT_FILL : 
                for (int i=0;i<nb;i++)
                    *dst++=*(pixel_t*)src; 
                src+=sizeof(pixel_t);
                break;
        }
    } while (!(header & 1<<5));
    o->c = (uintptr_t) src;
}

// fixme join with noclip through inline 
static void sprite3_line_clip (struct object *o)
{
    uint8_t * restrict src = (uint8_t *)o->c; 
    uint8_t * restrict dst = (uint8_t *)&draw_buffer[o->x]; 

    //message("clip line %d %d\n",vga_line-o->y, o->x);
    // fixme start with skip N times then std blit, then also unify with skipline 


    while(dst < (uint8_t *)&draw_buffer[o->x+o->w]) {
        uint8_t header = *src++; 
        const int nb = header & 63; // or nb bytes
        switch (header >> 6) {
            case BLIT_SKIP : // skip
                dst += nb;
                break;
            case BLIT_COPY : // literal
                if (dst>(uint8_t*)&draw_buffer[-32]) {
                    memcpy(dst,src,nb);
                }
                dst+=nb;
                src+=nb;
                break;
            case BLIT_BACK : // back reference as nb u16 :4, backref:10
                message("backref %d cpls\n",nb>>3);
                if (dst>(uint8_t*)&draw_buffer[-32]) {
                    const int delta = ((header&3)<<8 | *src)-2;
                    const int nnb = (nb>>3)*2;
                    memcpy(dst,src - delta, nnb);
                    dst += (nb>>3)*2; 
                    src += 1;
                }
                dst += (nb>>3)*2; 
                src += 1;
                break;
            case BLIT_FILL : // fill w / u16
                if (dst>(uint8_t*)&draw_buffer[-32]) {
                    for (int i=0;i<nb;i++) {                    
                        *dst++=src[0];
                        *dst++=src[1]; 
                    }
                } else {
                    dst += 2*nb;
                }
                src+=2;
                break;
        }
    }
    o->c = (uintptr_t) src;
    message("endl\n");
}

// any wide size
static void sprite3_cpl_line_noclip (object *o) {

    uint8_t *  restrict src=(uint8_t*)o->c;
    pixel_t *  restrict dst=draw_buffer+o->x; // u16 for vga8
    couple_t * restrict couple_palette = (couple_t *)o->b;

    uint8_t header;
    do {
        header=*src;

        int nb = read_len(&src);

        switch (header >> 6) {
            case BLIT_SKIP :
                //message("skip %d\n",nb);
                dst += nb;
                break;
            case BLIT_COPY:
                for (int i=0;i<nb/2;i++) {
                    *(couple_t*)dst = couple_palette[*src++];
                    dst += 2; // couple
                }
                
                if (nb%2) {
                    const couple_t last = couple_palette[*src++];
                    *dst++ = last >> 16;
                }                
                break;

            case BLIT_FILL : // fill w / u16
                for (int i=0;i<nb/2;i++) {                    
                    *(couple_t*)dst=couple_palette[*src];
                    dst +=2;
                }
                if (nb%2) {
                    *dst++ = couple_palette[*src] >> 16;
                } 
                src+=1;
                break;

            case BLIT_BACK : 
                {
                    const uint16_t delta = *(uint16_t*)(src);
                    for (int i=0;i<nb/2;i++) {
                        *(couple_t*)dst = couple_palette[(src-delta)[i]];
                        dst+=2;
                    }
                    if (nb%2) {
                        *dst++ = couple_palette[(src-delta)[nb/2]];
                    }
                    src+=2;
                }
                break;

        }
    } while (!(header & 1<<5)); // eol
    o->c=(intptr_t)src;
}