The Textmode library 
====================

Textmode is a simple color text mode library. 
You can choose the size of the font by including the library in your project and specifying FONT_W and FONT_H (defaulting to 8x8) to get a 256 font element. 

You can find current fonts under fonts/ directory, currently supporting

- 4x6 pixels
- 6x8 
- 8x16
and 
- 8x8 (default font).

you can then write to the vram memory, the mode is still defined by VGA_MODE in your build script, so the number of characters is defined by the font size and the vga mode.

For colors, there is a `vram_attr` array to write the different attributes. Those indices correspond to the pairs of pixels defined by 

    set_palette (index,foreground color,backgroun color)

By example if you do set_palette(1,white,red) and vram_attr[4][5]=1, the text at 4th line 5th column will be white on a red background.

(here red and white are a u16 or u8 pixel color)

There is an example in the main example distribution.

