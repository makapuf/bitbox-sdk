FrameBuffer library for the bitbox
==================================

The frame buffer library is a simple library where all you have to do is to write in RAM and the kernel outputs data to screen, so you just have to worry about your application.

You can configure the library by :

- specifying the mode in your makefile (VGA_MODE=320 by example)
- specifying the color depth with FRAMEBUFFER_BPP=N where N is 1,2,4 (default) or 8.


The size of the VRAM will be given by the formula : 

    VRAM size = x*y*BPP/8 (by example,  400x300 @ 4bpp is 60kB)

To use it : 

- write colors data to pixel_t 
- call clear()
- just write data to vram as bit-packed data (for 1BPP, the first byte of data represents 8 pixels)

There is an example in the bitbox repository, as well as utility functions `draw_line` and `draw_pixel`.

example program : 

	#define MAX_COLOR (1<<FRAMEBUFFER_BPP) // max color id

    void bitbox_main()
    {
    	clear();
    	// draw diagonal line
    	draw_line(0,0,100,100,1);
    	for (int i=0;;i++) {
    		for (int j=0;j<200;j++) {
    			wait_vsync(1);
    			draw_pixel(j,50,i%MAX_COLOR)
    		}
    	}
    }
