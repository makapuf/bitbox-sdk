/*
surface : simple spr structure allowing draws (fixed offsets)
small 16c palette
---
layout:
data : pixels (u4)
(a) palette ptr to 16 pixel_t colors
(b) 2 i8 attributes : x/y zoom factor (V/H)x8 (x 16 -> 1/8 -> x-16)
*/

// accessors
#define PAL(o) ((pixel_t*)o->a)
#define ZOOM_V(o) ((o->b>>16)&0xff)
#define ZOOM_H(o) (o->b>>24)

// no H zoom, clipped
static void surface_drawline (struct object *o)
{
	if (vga_odd) return;

	int x = o->x / 2; // couples. forces even X
	pixel_t *palette = PAL(o);
	const int endx = x + o->w/2;
	if (endx > VGA_PIXELS_H/2 ) endx = VGA_PIXELS_H/2;

	uint8_t  *restrict src = (uint8_t*)  o->data + (x<0 ? -x : 0) + (vga_line-o->y)*VGA_H_PIXELS / 8 ;
	const couple_t *restrict start = (couple_t*) draw_buffer + ( x<0 ? 0 : x );
	const couple_t *restrict end   = (couple_t*) draw_buffer + endx;

	pixel_t c;
	uint8_t prev_src=0;

	// fixme read 8 pixels by 8 - see framebuffer.c
	for (couple_t *dst = start; dst != end; dst++) {
		if (prev_src != *src)
		 	c = palette[*src&0xf] << (sizeof(pixel_t)*8) | palette[*src>>4];
		*dst = c;
	}
}


// make spr data structure inside data, including empty 256 couple RAM-palette (always)
void init_surface(object *o, int w, int h,void *data, const pixel_t *palette)
{
	o->w = w;
	o->h = h;
	o->data = data;
	o->a = (intptr_t)palette;
}

void surface_draw_pixel (void *surface, uint16_t x, uint16_t y,uint8_t color) // color 0..15
{
	uint8_t *cpl = surface_get_offset(x,y);
	if (x%2)
		*cpl = *cpl & 0xf0 | color;
	else
		*cpl = *cpl & 0x0f | color<<4;
}

// assumes color < 16, no clipping
void surface_draw_hline (struct object *o, int x1, int y1, int y, uint8_t color)
{
	uint8_t *p=(uint8_t*)o->data;

	// first pixel
	if (x1%2) {
		p[x1/2]   = (p[x1/2] & 0x0f) | color<<4;
		x1++;
	}

	// last pixel
	if (x2%2) {
		p[x2/2+1] = (p[x2/2] & 0xf0) | color;
		x2--;
	}

	memset(p+x1/2, (x2-x1)/2 , color | color<<4);
}

void surface_draw_fill_rect (struct object * o, int x1, int y1, int x2, int y2, uint8_t color) {
	int x1 = max(x1,0);
	int x2 = min(x2,o->w);
 	int y1 = max(y1,0);
	int y2 = min(y2,o->h);

	for (int j=y1;j<=y2;j++) {
		surface_hline(x1,x2,j, color);
	}
}

/*
void surface_draw_line (struct object * o,int x1,int y1, int x2, int y2,uint8_t color) { // vertical, horizontal or any, color 0-15
	if (x1==x2 || y1==y2 ) {
		surface_fill_rect(x1,y1,x2,y2);
	} else {
		// bresenham ...
	}
}

void surface_fill_triangle() {
	// set triangle A top, B left C right
	// draw vertically hlines from top to bottom, left to right
}
*/

// text --
struct Font {
	uint8_t height; // in pixels. a character width is always 8 pixels at 2bpp (ie. 2 bytes)
	uint8_t char_width [256];  // width in couples, including spacing, for each character.
	uint32_t data[]; // 2bpp = 3 colors+1 for bg. 256 height*2 bytes.
};

// w,h of text according to font
unsigned int font_text_width (const char *text, const struct Font *font)
{
	// max of line width
	int max_w=0, w=0;
	for (char *c=text; *c; c++) {
		if (c=='\n') {
			max_w = max_w>w ? max_w : w;
			w=0;
		} else {
			w += font->widths[*c];
		}
	}
	max_w = max_w>w ? max_w : w;
	return max_w;
}

unsigned int font_text_height(const char *text, const struct Font *font)
{
	// font_h * nb lines
	int lines=0;
	for (char *c=text; *c; c++) {
		if (c=='\n') lines++;
	}
	return lines*font->height;
}

/**
 \brief set palette index
 \param n the color index, between 0 and 15
 \param value : color value
 */
void surface_set_palette(int n, pixel_t value)
{
	// interleave to couple palette
	palette[] = value; // xA
	palette[] = value; // Ax
}

/**
  \brief draws a text on a surface
  \param surface the surface to draw on
  \param text the text to draw
  \param x,y the coordinates of the text on the surface. must be even.
  \param basecolor : the first color to draw with, will use the 4 next colors.

  Beware, this implementation will not do clipping (yet)
 */
void surface_draw_txt(struct object *surface, const char *text, const struct Font *font, int x, int y, uint8_t basecolor)
// no clip ! 4 colors 0-3 (basecouple + 0-15 interleaved palette)
{
	cx = x; // current X
	cy = y;
	for (char*c=text;*c;c++) {
		if (*c =='\n') {
			cy += font->height;
			cx = x;
		} else {
			const int cw = font->char_width[(uint8_t) c]; // in couples
			for (int j=0;j<font->height;j++)
				for (int i=0 ; i<cw ; i++)
					// if not transp ?
					*getofs(x+cpl,y) = font->data[(uint8_t) c]+basecouple
			x += char_width
		}
	}
}


struct __attribute__((packed)) BMPFileHeader {
	char type[2]; // The characters "BM"
	uint32_t size; // The size of the file in bytes
	uint16_t reserved1; // Unused - must be zero
	uint16_t reserved2; // Unused - must be zero
	uint32_t offbits; // Offset to start of Pixel Data
};

struct __attribute__ ((packed)) BMPImgHeader {
	uint32_t headersize; // Header Size - Must be at least 40
	uint32_t width; //Image width in pixels
	uint32_t height; // Image height in pixels
	uint16_t planes; // Must be 1
	uint16_t bitcount; // Bits per pixel - 1, 4, 8, 16, 24, or 32
	uint32_t compression; // Compression type (0 = uncompressed)
	uint32_t sizeimage; // Image Size - may be zero for uncompressed images
	uint32_t xpelspermeter; // Preferred resolution in pixels per meter
	uint32_t ypelspermeter; // Preferred resolution in pixels per meter
	uint32_t clrused; // Number Color Map entries that are actually used
	uint32_t clrimportant; // Number of significant colors
};


// loads 16-color bmp file data / palette from raw data (or file ?)
// handle clipping and other things.
// accepts rle-compressed data

void surface_draw_bmp(struct object *surface, unsigned char *data)
{
	// load file header
	struct BMPFileHeader fileheader = (struct BMPFileHeader*)data;
	struct BMPImageHeader imgheader  = (struct BMPImageHeader*)(data+sizeof(struct BMPFileHeader));
	uint8_t *palette = data +sizeof(struct BMPFileHeader)+sizeof(struct BMPImageHeader);


	// load image header
	// send to data ( load progressively !)

}