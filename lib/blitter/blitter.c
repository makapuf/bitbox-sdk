#include "blitter.h"

#include <stdint.h>
#include <stddef.h> // NULL
#include <string.h> // memset
#include "utlist.h"

#ifndef EMULATOR
#include "stm32f4xx.h" // profile
#endif

#if BITBOX_KERNEL != 0010
#error must be compiled with kernel version v0.10
#endif

#ifndef MAX_OBJECTS
#define MAX_OBJECTS 64
#endif

extern int line_time;

typedef struct {
    object *toactivate_head; // top of display list, sorted by Y. not yet active.
    object *active_head;     // top of the active list (currently active on this line), sorted by Z.
    object *inactive_head;   // inactive objects, to be activated next frame. this list is not sorted- shall be almost
} Blitter;

Blitter blt CCM_MEMORY = {0,0,0} ;

static inline int cmp_y(object *o1, object *o2) { return o1->y < o2->y ? -1 : (o1->y == o2->y ? 0 : 1) ; }
static inline int cmp_z(object *o1, object *o2) { return o1->z > o2->z ? -1 : (o1->z == o2->z ? 0 : 1) ; }


static void __attribute__((unused)) blitter_print_state(char *str)
{
    message(" --- %s : frame %d line %d \n",str,vga_frame, vga_line);
    message("to activate: ");   for ( object *o=blt.toactivate_head;o; o = o->next) message("%x - ", o); message("\n");
    message("active: ");        for ( object *o=blt.active_head    ;o; o = o->next) message("%x - ", o); message("\n");
    message("inactive: ");      for ( object *o=blt.inactive_head  ;o; o = o->next) message("%x - ", o); message("\n");
}
// insert to blitter. not yet active
void blitter_insert(struct object *o, int16_t x, int16_t y, int16_t z)
{
    o->x=x; o->y=y; o->z=z;

    // prepend since we don't care of order and it's faster
    LL_PREPEND(blt.inactive_head, o);
}

static void ensure_in(object *o, object *head, char *str) {
    object *elt;
    LL_FOREACH(head,elt) {
        if (o==elt) {
            message ("found & removed object %x\n",o);
            return;
        }
    }
    message ("ERROR : object %x NOT FOUND in %s\n",o,str);
    blitter_print_state("not found in:");
    die(1,1);
}

void blitter_remove(object *o)
{
    message("removing %x y=%d h=%d, vga_line=%d : ",o, o->y, o->h, vga_line);
    // object should not be in its active zone
    if ((int)vga_line<o->y) { // not yet reached
        ensure_in(o,blt.toactivate_head,"toactivate");
        LL_DELETE(blt.toactivate_head, o);
    } else if ((int)vga_line>o->y+o->h) {
        ensure_in(o,blt.inactive_head,"inactive");
        LL_DELETE(blt.inactive_head, o);
    } else if ((int)vga_line>=VGA_V_PIXELS) { // OK object was not removed but we're past screen display
        ensure_in(o,blt.active_head,"active");
        LL_DELETE(blt.active_head, o);
    } else {
        // we're in active zone, danger !
        die(4,3);
        message ("ERROR : cannot remove active object from blitter yet !\n");
    }
}

void graph_vsync()
{
    if (vga_odd)
        return;

    struct object *o;

    switch (vga_line) {
        case VGA_V_BLANK-3 :
            // append active, inactive lists to to_activate
            if (blt.active_head)   LL_CONCAT(blt.toactivate_head, blt.active_head);
            if (blt.inactive_head) LL_CONCAT(blt.toactivate_head, blt.inactive_head);

            // empty them
            blt.active_head   = 0;
            blt.inactive_head = 0;

            // sort to_activate along Y (should be almost sorted)
            LL_SORT(blt.toactivate_head, cmp_y);
            break;

        case VGA_V_BLANK-2 :
            // rewind all objects to activate
            LL_FOREACH(blt.toactivate_head, o) {
                if (o->frame)
                    o->frame(o,o->y<0?-o->y:0); // first line is -y if negative
            }
            break;
    }
}


// drop past objects from active list, remove them from active list + move them to inactive list
static inline void drop_old_objects ()
{
    object *prev=NULL;
    for (object *o=blt.active_head;o;)
    {
        object *next = o->next;
        if ((int)vga_line >= o->y+(int)o->h) {
            // remove this object from active list, append to inactive
            // no need to scan to find previous, we just got it.
            if (o==blt.active_head) {
                blt.active_head = next; // change head, still no previous
            } else {
                prev->next=next;  // just drop from list
            }
            LL_PREPEND(blt.inactive_head, o); // change o-next
            // do not change prev
        } else {
            prev=o;
        }
        o = next;
    }
}

void graph_line()
{
    // persist between calls so that one line can continue blitting objects next semi-line. cut is done at z=128
    static object *o;

    if (!vga_odd) { // only on even lines

    // add new active objects
    while (blt.toactivate_head && (int)vga_line>=blt.toactivate_head->y)
    {
        object *next = blt.toactivate_head->next;
        LL_INSERT_INORDER(blt.active_head,blt.toactivate_head, cmp_z); // modifies head->next
        blt.toactivate_head = next; // remove head from list, take next
    }

    drop_old_objects(); // also drops just added but too late

    // now trigger each element of activelist, in Z descending order
    LL_FOREACH(blt.active_head,o) {
        #ifdef VGA_SKIPLINE // multiline blit
        if (o->z<128) break; // stop here, will finish on odd line
        #endif
        o->line(o);
    }

    } else { // odd
        // continue with o
        for (;o;o=o->next)
            o->line(o);
    }
}


void fast_fill(uint16_t x1, uint16_t x2, uint16_t c)
{
    // ensures start is 32bit-aligned
    if (x1 & 1)
    {
        draw_buffer[x1]=c;
        x1++;
    }

    // ensures end is written if unaligned
    if (!(x2 & 1)) {
        draw_buffer[x2]=c; // why this +1 ????
        x2--;
    }

    // 32 bit blit, manually unrolled
    uint32_t * restrict dst32 = (uint32_t*)&draw_buffer[x1];
    int i=(x2-x1)/2;

    for (;i>=8;i-=8)
    {
        *dst32++ = c<<16 | c;
        *dst32++ = c<<16 | c;
        *dst32++ = c<<16 | c;
        *dst32++ = c<<16 | c;
        *dst32++ = c<<16 | c;
        *dst32++ = c<<16 | c;
        *dst32++ = c<<16 | c;
        *dst32++ = c<<16 | c;
    }

    for (;i>=0;i--)
        *dst32++ = c<<16 | c;
} __attribute__((hot))


// --- misc implementations & tests

void color_blit(object *o)
{
    const int16_t x1 = o->x<0?0:o->x;
    const int16_t x2 = o->x+o->w>VGA_H_PIXELS ? VGA_H_PIXELS : o->x+o->w;

    #if VGA_BPP==8
    memset(&draw_buffer[x1],o->a,x2-x1);
    #else
    fast_fill(x1,x2,o->a);
    #endif
}

void rect_init(struct object *o,uint16_t w, uint16_t h, pixel_t  color)
{
    o->w=w; o->h=h;

    o->a = (uint32_t)color;

    o->frame=0;
    o->line=color_blit;
}

