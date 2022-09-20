#include "framebuffer.h"

#include <stdint.h>
#include <bitbox.h>
#include <stdlib.h> // abs
#include <string.h> // memset


uint32_t vram[VGA_H_PIXELS*VGA_V_PIXELS*FRAMEBUFFER_BPP/32];
pixel_t palette[1<<FRAMEBUFFER_BPP];

#define PPW (32/FRAMEBUFFER_BPP) // source pixels per word

// --------------------------------------------------------------

#if FRAMEBUFFER_BPP==4 
// 320x240 = 38k 
pixel_t initial_palette[]  = {
	RGB(   0,   0,   0), RGB(   0,   0,0xAA), RGB(   0,0xAA,   0), RGB(   0,0xAA,0xAA),
	RGB(0xAA,   0,   0), RGB(0xAA,   0,0xAA), RGB(0xAA,0x55,   0), RGB(0xAA,0xAA,0xAA),
	RGB(0x55,0x55,0x55), RGB(0x55,0x55,0xFF), RGB(0x55,0xFF,0x55), RGB(0x55,0xFF,0xFF),
	RGB(0xFF,0x55,0x55), RGB(0xFF,0x55,0xFF), RGB(0xFF,0x55,0x55), RGB(0xFF,0xFF,0xFF),
};

void graph_line() {
	if (vga_odd) return;
	uint32_t *src=&vram[(vga_line)*VGA_H_PIXELS/8];
	uint32_t *dst=(uint32_t*)draw_buffer;

	for (int i=0;i<VGA_H_PIXELS/8;i++) {
		uint32_t w = *src++; // read 1 word = 8 pixels

		for (int j=0;j<32;j+=16) { // 2 quads of pixels 
			uint32_t q;
			q  = palette[w>>(j+12)&7]<<24;
			q |= palette[w>>(j+ 8)&7]<<16;
			q |= palette[w>>(j+ 4)&7]<<8;
			q |= palette[w>>j & 7];
			*dst++ = q;
		}	
	}
}

// --------------------------------------------------------------

#elif FRAMEBUFFER_BPP==8 
// bitbox 640x480 is not feasible
// 400x300 is 120k in that mode ! 
// 320x240 = 76k 

const uint16_t initial_palette[] = { // 256 colors standard VGA palette
	// XXX replace with micro palette
	0x0000, 0x0015, 0x02a0, 0x02b5, 0x5400, 0x5415, 0x5540, 0x56b5,
	0x294a, 0x295f, 0x2bea, 0x2bff, 0x7d4a, 0x7d5f, 0x7fea, 0x7fff,
};

void graph_line() {
	if (vga_odd) return;
	uint32_t *dst=(uint32_t*)draw_buffer;
	uint32_t *src=&vram[vga_line*VGA_H_PIXELS/4];
	memcpy(dst,src,VGA_H_PIXELS);
}
#endif

// --------------------------------------------------------------
// utilities


void clear()
{
	memset(vram, 0, sizeof(vram));
	memcpy(palette,initial_palette,sizeof(initial_palette));
}

void draw_pixel(int x,int y,int c)
{
	int pixel=x+y*VGA_H_PIXELS; // number of the pixel
	vram[pixel/PPW] &= ~ (((1<<FRAMEBUFFER_BPP)-1)<<(FRAMEBUFFER_BPP*(pixel%PPW))); // mask
	vram[pixel/PPW] |= c<<(FRAMEBUFFER_BPP*(pixel%PPW)); // value
	// if e.g. BPP == 2 (640x400 mode)
	// you fit 32/BPP = 16 pixels in one 32bit integer
}

void draw_line(int x0, int y0, int x1, int y1, int c) {
	int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
	int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1;
	int err = (dx>dy ? dx : -dy)/2, e2;

	for(;;){
		draw_pixel(x0,y0,c);
		if (x0==x1 && y0==y1) break;
		e2 = err;
		if (e2 >-dx) { err -= dy; x0 += sx; }
		if (e2 < dy) { err += dx; y0 += sy; }
	}
}

// draw text at 8x8 ?