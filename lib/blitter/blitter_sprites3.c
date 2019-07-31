// blitter_sprites3.c : simpler blitter objects. .
// All in 8bpp, define vga_palette symbol as uint16_t [256] to expand later

/* object layout in memory :
 data : raw 8/16b data (start of blits)
 a : start of raw source data
 b : palette if couple palette
 d : if hi u16 not zero, replace with this color. 
     if d&1 render with size = 2x (modify w/h as needed!)
     if d&2 make it invisible
 */

#include <string.h> // memcpy, memset
#include <stdbool.h>
#include "blitter.h"

void sprite3_frame(struct object *o, int line);

void sprite3_line_noclip         (struct object *o);
void sprite3_line_clip           (struct object *o);
void sprite3_cpl_line_clip       (struct object *o);
void sprite3_cpl_line_noclip     (struct object *o);
void sprite3_cpl_line_noclip_2X  (struct object *o);
void sprite3_cpl_line_solid      (struct object *o);
void sprite3_cpl_line_solid_clip (struct object *o);
void skip_line                   (struct object *o);

#define DATACODE_u16 0
#define DATACODE_u8 1
#define DATACODE_c8 2

#define BLIT_SKIP 0
#define BLIT_FILL 1
#define BLIT_COPY 2
#define BLIT_BACK 3

#define MARGIN 64

#if VGA_BPP==8
typedef uint16_t couple_t;
#else
typedef uint32_t couple_t;
#endif

void sprite3_load(struct object *o, const void *data)
{
    const struct SpriteFileHeader *h=data;

    if (h->magic!=0xB17B) {
    	message("Error : wrong header found");
    	bitbox_die(8,8);
    }

    o->w = h->width;
    o->h = h->height;

    o->a = (uintptr_t)h;
    if (h->datacode == DATACODE_c8) {
        uint32_t *p = (uint32_t *)&h->data[h->frames*h->height];
        uint32_t palette_len = *p++;
        o->b = (uintptr_t) p; // real start of palette
        o->data = (void *) (p+palette_len); // after start of palette
    } else {
        o->b=0;
        o->data = (void*) &h->data[h->frames*h->height];
    }

    o->frame = sprite3_frame;
    o->line = skip_line; // skip now until frame decides which one to use

    // default values
    o->fr=0;
    o->d=0; // reset MODE
}

void sprite3_set_solid(object *o, pixel_t color)
{
    o->d = color << 16 | (o->d & 0xffff);
}

void sprite3_toggle2X(object *o)
{
    if (o->d&1) { // 2X was set
        o->w /= 2;
        o->h /= 2;
        o->d &= ~1;
    } else {
        if (o->b) {// for now only with cpls
            o->w *= 2;
            o->h *= 2;
            o->d |= 1;
        }
    }
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

void sprite3_frame(struct object *o, int start_line)
{
    if (o->x + (int)o->w < 0 || o->x > VGA_H_PIXELS || o->d & 2 ) { // non visible X : skip rendering this frame
        o->line = skip_line;
        return;
    }

    // select if clip or noclip (choice made each frame)
    if (o->d & 1) {  // 2X rendering - only for couples for now
        o->line = sprite3_cpl_line_noclip_2X; // fixme clipping
        return;
    }

    if ( o->x < -MARGIN || o->x + (int)o->w >= VGA_H_PIXELS+MARGIN ) { // clip ?
        // cpl, solid or full pixels ?
        if (o->b) { // has a palette
            o->line = o->d & 0xffff0000 ? sprite3_cpl_line_solid_clip : sprite3_cpl_line_clip;
        } else {
            o->line = sprite3_line_clip;
        }
    } else {
        if (o->b) {
            o->line = o->d & 0xffff0000 ? sprite3_cpl_line_solid : sprite3_cpl_line_noclip;
        } else {
            o->line = sprite3_line_noclip;
        }
    }
}

void skip_line(struct object *o) {}

void sprite3_line_noclip (struct object *o)
{
    struct SpriteFileHeader *h = (struct SpriteFileHeader*)o->a;
    const uint16_t line = (int)o->fr*h->height+vga_line-o->y;
    uint8_t * restrict src = (uint8_t*) o->data + h->data[line];
    pixel_t * restrict dst = &draw_buffer[o->x];

    uint8_t header;
    uint16_t backref;

    do {
        header = *src;

        const int nb = read_len(&src);

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
                backref = *(uint16_t*)(src);
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
}

// fixme join with noclip through inline, allow partial skips like next one
void sprite3_line_clip (struct object *o)
{
    struct SpriteFileHeader *h = (struct SpriteFileHeader*)o->a;
    const uint16_t line = (int)o->fr*h->height+vga_line-o->y;
    uint8_t * restrict src = (uint8_t*) o->data + h->data[line];
    pixel_t * restrict dst = (pixel_t *)&draw_buffer[o->x];

    uint8_t header;

    do {
        header = *src;

        const int nb = read_len(&src);

        switch (header >> 6) {
            case BLIT_SKIP : // skip
                dst += nb;
                break;
            case BLIT_COPY : // literal
                if (dst>draw_buffer-MARGIN) {
                    memcpy(dst,src,nb);
                }
                dst+=nb;
                src+=nb*sizeof(pixel_t);
                break;
            case BLIT_BACK : // back reference as u16
                if (dst>draw_buffer-MARGIN) {
                    const int backref = *(uint16_t*)(src);
                    memcpy(dst,src - backref, nb);
                }
                src += 2;
                dst += nb;
                break;
            case BLIT_FILL : // fill w / u16
                if (dst>draw_buffer-MARGIN) {
                    for (int i=0;i<nb;i++) {
                        *dst++=*(pixel_t*)src;
                    }
                } else {
                    dst += nb;
                }
                src+=sizeof(pixel_t);
                break;
        }
    } while (!(header & 1<<5) && dst < &draw_buffer[o->x+o->w]); // eol
}

static inline __attribute__((always_inline)) void sprite3_cpl_line (object *o, bool clip, bool solid)
{
    // Skip to line
    struct SpriteFileHeader *h = (struct SpriteFileHeader*)o->a;
    const uint16_t line = o->fr*h->height+vga_line-o->y;
    uint8_t *  restrict src=(uint8_t*) o->data + h->data[line];

    pixel_t *  restrict dst=draw_buffer+o->x; // u16 for vga8
    couple_t * restrict couple_palette = (couple_t *)o->b;

    uint8_t header;
    couple_t solidcolor = (o->d>>16) * 0x10001;

    // clip left : skip runs fixme finish partial run ?
    if (clip) {
        do {
            header=*src;
            int nb = read_len(&src);
            dst += nb;
            switch (header>>6) {
                case BLIT_SKIP :
                    break;
                case BLIT_COPY:
                    src += (nb+1)/2;
                    break;
                case BLIT_FILL : // fill w / u16
                    src+=1;
                    break;
                case BLIT_BACK :
                    src+=2;
                    break;
            }
        } while (dst<draw_buffer-MARGIN);
    }

    do {
        header=*src;
        int nb = read_len(&src);

        switch (header >> 6) {
            case BLIT_SKIP :
                dst += nb;
                break;
            case BLIT_COPY:
                for (int i=0;i<nb/2;i++) {
                    *(couple_t*)dst = solid ? solidcolor : couple_palette[*src];
                    src++;
                    dst += 2; // couple
                }

                if (nb%2) {
                    const couple_t last = solid ? solidcolor : couple_palette[*src];
                    src++;
                    *dst++ = last >> 16;
                }
                break;

            case BLIT_FILL : // fill w / u16
                for (int i=0;i<nb/2;i++) {
                    *(couple_t*)dst= solid ? solidcolor : couple_palette[*src];
                    dst +=2;
                }
                if (nb%2) {
                    *dst++ = solid ? solidcolor : couple_palette[*src] >> 16;
                }
                src+=1;
                break;

            case BLIT_BACK :
                {
                    const uint16_t delta = *(uint16_t*)(src);
                    for (int i=0;i<nb/2;i++) {
                        *(couple_t*)dst = solid ? solidcolor : couple_palette[(src-delta)[i]];
                        dst+=2;
                    }
                    if (nb%2) {
                        *dst = solid ? solidcolor : couple_palette[(src-delta)[nb/2]];
                        dst++;
                    }
                    src+=2;
                }
                break;
        }
    } while (!(header & 1<<5) && dst < &draw_buffer[o->x+o->w]); // eol
}

void sprite3_cpl_line_clip   (object *o) { sprite3_cpl_line(o,true,  false); }
void sprite3_cpl_line_noclip (object *o) { sprite3_cpl_line(o,false, false); }
void sprite3_cpl_line_solid  (object *o) { sprite3_cpl_line(o,false, true); }
void sprite3_cpl_line_solid_clip (object *o) { sprite3_cpl_line(o,false, true); }


static inline void blit2Xcpl(pixel_t *dst, couple_t color)
{
    #if VGA_BPP == 16
    *(couple_t*)dst = (color&0xffff)*0x10001;
    *(couple_t*)(dst+2) = (color >>16)*0x10001;
    #else
    *(couple_t*)dst = (color&0xff)*0x101;
    *(couple_t*)(dst+2) = (color >>8)*0x101;
    #endif
}
static inline void blit2Xsingle(pixel_t *dst, couple_t color)
{
    #if VGA_BPP == 16
    *(couple_t*)dst = (color&0xffff)*0x10001;
    #else
    *(couple_t*)dst = (color&0xff)*0x101;
    #endif
}

// This one has doubled size
void sprite3_cpl_line_noclip_2X (object *o) {

    // Skip to line
    struct SpriteFileHeader *h = (struct SpriteFileHeader*)o->a;
    const uint16_t line = o->fr*h->height+(vga_line-o->y)/2;
    uint8_t *  restrict src=(uint8_t*) o->data + h->data[line];

    pixel_t *  restrict dst=draw_buffer+o->x; // u16 for vga8
    couple_t * restrict couple_palette = (couple_t *)o->b;

    uint8_t header;
    do {
        header=*src;

        int nb = read_len(&src);
        couple_t c;
        switch (header >> 6) {
            case BLIT_SKIP :
                dst += nb*2;
                break;
            case BLIT_COPY:
                for (int i=0;i<nb/2;i++) {
                    c=couple_palette[*src++];
                    blit2Xcpl(dst,c);
                    dst += 4; // couple
                }
                if (nb%2) {
                    blit2Xsingle(dst,couple_palette[*src++]);
                    dst += 2;
                }
                break;

            case BLIT_FILL :
                c=couple_palette[*src++];
                for (int i=0;i<nb/2;i++) {
                    blit2Xcpl(dst,c);
                    dst += 4; // couple
                }

                if (nb%2) {
                    blit2Xsingle(dst,c);
                    dst+=2;
                }
                break;

            case BLIT_BACK :
                {
                    const uint16_t delta = *(uint16_t*)(src);
                    src+=2;

                    for (int i=0;i<nb/2;i++) {
                        couple_t c = couple_palette[(src-delta)[i]];
                        blit2Xcpl(dst,c);
                        dst+=4;
                    }
                    if (nb%2) {
                        couple_t c = couple_palette[(src-delta)[nb/2]];
                        blit2Xsingle(dst,c);
                        dst+=2;
                    }
                }
                break;

        }
    } while (!(header & 1<<5)); // eol
}