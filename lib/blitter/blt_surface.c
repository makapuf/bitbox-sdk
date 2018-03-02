/*
TODO
fixer les non couples avec les line header !!
blitter sprite avec zoom vertical (free, peut mm etre négatif)

passer les coordonnées en subpixel 1/16 ?

horizontal  : plus dur (moins free) -> dispatch line draw via frame - inline always



---
layout:
data : pixels (u4)


*/
// --- manip de palette

// fade 256 cpls palette to black
uint16_t *palette_fromspr(const object *o) {return (uint16_t*) o->b; } // gets palette from sprite
palette_fade_black(const u16 *src,u16 *dst,u8 level)
palette_replace(const u16 *src, u16 *dst, u16 from, u16 to);

/*
	interleave second pixels of 16 couples to 256 couples
	interleaves inplace, idempotent
	xA xB .. -> AA, AB, ... BA,BB, ...
*/
void palette_interleave(uint16_t *palette[])
{
	for (int i=0;i<16;i++) {
		for (j=0;j<16;j++) {
			palette[2*(i*16+j)]   = palette[j*2];
			palette[2*(i*16+j)+1] = palette[i*2];
		}
	}
}

int palette_from_bmp(); // loads 16-palette from BMP file, interleave it.

// surface : simple spr structure allowing draws (fixed offsets)
// 16c interleaved into 256cpls, unique lines, 1 blit de w/2 couples per line

// make spr data structure inside data, including empty 256 couple RAM-palette (always)
void make_cplsurface(w,h,void *data)
{
	// create sprite header w/ palette, data
	// create linerefs
	// create empty blits
}
// loader ensuite avec new_sprite3(data,x,y,z);

inline uint8_t *surface_get_offset(void *data, uint16_t x, uint16_t y)
// get offset of couple ref. if x is odd  must draw higher nibble
{
	const int header_sz = 256*2+12;
	return (uint8_t *)data + header_sz + (width(data)/2+2)*y+x/2;
}

void surface_draw_pixel (void *surface, uint16_t x, uint16_t y,uint8_t color) // color 0..15
{
	uint8_t *cpl = surface_get_offset(x,y);
	if (x%2)
		*cpl = *cpl & 0xf0 | color;
	else
		*cpl = *cpl & 0x0f | color<<4;
}

surface_draw_line (void *surface,x1,y1,x2,y2,u8 color); // vertical, horizontal, any. color 0-15
surface_bltbmp (void *surface, *bmpfile) // draw 16c bmp image, including transparency and clipping. RLE support ?
	// dont change palette (séparé)


// text --
struct Font {
	uint16_t magic;
	uint8_t rfu;
	uint8_t height;
	uint8_t widths[256];
		// width in couples (even if max width is 4 couples, could be u2)
		// include space after (char plutot pairs -> 1 px espace -> cpls)
	uint32_t data [256][];
		// liste de height times 256-charlines (line-interleaved)
		// charline : 8 x u4 couple_refs ds 4x4 palette per character = 32bits
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
	for (char *c=text;, *c; c++) {
		if (c=='\n') lines++;
	}
	return lines*font->height;
}

void surface_draw_txt(surface, text,x,y,basecouple)
// no clip ! 4 colors 0-3 (basecouple + 0-15 interleaved palette)
{
	// build trans_palette from basecouple to real as 16c palette
	// now blit
	for char in text :
		if char =='\n' :
			y += size of 8 lines
			x = startx
		else :
			if char ='\n' line +=1, continue
				for cpl in char_width :
					for txtline in 0..font_h :
						y +=
						getofs(x+cpl,y) = fontdata[char]+basecouple
			x += char_width
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


// loads 16-color bmp file data / palette from raw data
// accepts rle-compressed data. always put to RAM,
// for in memory compressed files use sprites

void surface_load_bmp(struct object *surface, unsigned char *data)
{
	// load file header
	struct BMPFileHeader *fileheader = (struct BMPFileHeader*)data;
	struct BMPImageHeader *imgheader  = (struct BMPImageHeader*)(data+sizeof(struct BMPFileHeader));
	uint8_t *palette = data +sizeof(struct BMPFileHeader)+sizeof(struct BMPImageHeader);


	// load image header
	// send to data

}