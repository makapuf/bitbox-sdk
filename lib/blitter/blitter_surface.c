/* surface2bpp blitter object : 2bpp surface, non clipped.
width must be a multiple of 16 !

 layout :
 	data : 4x4 interleaved couples palette (aa,ab,ac,ad,ba,...) , then 2bpp pixels
 	a,b,c,d : unused for now : zoom h,v ?
 */

#include "blitter.h"

#if VGA_BPP==8
typedef uint16_t couple_t;
#else
typedef uint32_t couple_t;
#endif
static void surface_line (struct object *o);

void surface_init (struct object *o, int _w, int _h, void *_data)
{
	o->w=_w; o->h=_h; o->data = _data;
	o->line = surface_line;
	o->frame=0;

	surface_clear(o);
}

void surface_setpalette (struct object *o, const pixel_t *pal)
{
	couple_t *p = (couple_t*) o->data;
	for (int i=0;i<4;i++)
		for (int j=0;j<4;j++) {
			p[j+4*i] = pal[i] << (8*sizeof(pixel_t)) | pal[j];
		}
}

// no H zoom, not clipped yet
static void surface_line (struct object *o)
{
	couple_t *palette = (couple_t *) o->data;

	// would use restrict if C
	uint8_t  * src = (uint8_t*) o->data + 16*sizeof(couple_t) + ((vga_line-o->y)*o->w) / 4;
	couple_t * start = (couple_t*) draw_buffer + o->x/2 ;
	couple_t * end   = (couple_t*) draw_buffer + (o->x+o->w)/2;

	uint8_t oldsrc = *src+1; // not src so first time they WILL updated.

	couple_t c1,c2;
	for (couple_t *dst = start; dst < end; src++, dst+=2) {
		if (*src != oldsrc) { // avoid palette fetch if same src
		 	c1 = palette[*src & 0xf];
		 	c2 = palette[*src >> 4];
		 	oldsrc = *src;
		}
		#pragma GCC diagnostic push
 		#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	 	dst[0] = c1;
		dst[1] = c2;
		#pragma GCC diagnostic pop
	}
}

// color between 0 and 3
void surface_fillrect (struct object *o, int x1, int y1, int x2, int y2, uint8_t color)
{
	if (x1<0 || y1<0 || x1>=x2 || y1>=y2 || x2>o->w || y2>=o->h) {
		message("wrong arguments : %d,%d %d,%d on surface fill %d,%d\n",x1,y1,x2,y2,o->w,o->h);
		bitbox_die(7,7);
	}
	const uint32_t wc = (color & 3)*0x55555555; // repeat 16 times -> no line not multiple of 16 !
	uint32_t *p = (uint32_t *)o->data + 4*sizeof(couple_t) + o->w/16*y1; // start of line, in words

	for (int y=y1; y<y2;y++) {

		// set last bits of word
		const int nbits = (x1%16) * 2;
		p[x1/16] = (p[x1/16] & (0xffffffffUL << (32-nbits))) | (wc << nbits);

		// fill whole words
		for (int i=x1/16+1;i<x2/16;i++) p[i] = wc;

		// set first bits of word - if any
		const int nbits2 = (x2%16) * 2;
		p[x2/16] = (p[x2/16] & (0xffffffffUL << nbits2)) | (wc >> (32-nbits2));

		p += o->w/16; // next line
	}
}

void surface_clear(struct object *o) { 
	surface_fillrect(o,0,0,o->w-1,o->h-1,0);
}

int surface_char (struct object *o, const char c, int x, int y, const void *fontdata)
{
	const struct Font* font = fontdata;
	uint8_t *p = (uint8_t *)o->data + 16*sizeof(couple_t); // buffer start
	const uint8_t ch = (uint8_t)c-' ';
	const int cw = font->char_width[ch];
	for (int j=0;j<font->height;j++) {
		const uint32_t *cp = (uint32_t *)&font->data[(ch*font->height + j)*font->bytes_per_line]; // source word address
		uint32_t src_pixels = *cp; //read them
		src_pixels &= 0xffffffff >> (8*(4-font->bytes_per_line)); // mask source : only read bpl pixels
		uint32_t *dst = (uint32_t *)&p[(o->w/4)*(y+j)+x/4]; // existing word, byte aligned
		uint32_t pw = *dst; // read existing pixels. at most 32bits so 16pixels wide
		//pw &= 0xffffffff << (cw*2); // mask it - or not if transparent render ?
		pw |= src_pixels << ((x%4)*2); // read 32 bits, shift them left (ie pixels to right) to the right place
		*dst = pw; // write it back
	}
	return cw;
}


// opaque text, non wrapped, non clipped.
void surface_text (struct object *o, const char *text, int x, int y,const void *fontdata)
{
	const struct Font* font = fontdata;
	int cx = x; // current X
	for (const char *c=text ; *c ; c++) {
		if (y+font->height > o->h)
			break;

		if (*c =='\n') {
			y += font->height+1;
			cx = x;
		} else if (*c=='\t') {
			cx = (cx+32)/32*32;
		} else if (*c==' ') {
			cx += 2;
		} else {
			if (cx>o->w-4) {
				y+=font->height+1;
				cx=x;
			}
			cx += surface_char(o, *c, cx, y, fontdata)+1;
		}
	}
}
