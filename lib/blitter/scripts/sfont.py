"""make proportional fonts from a bitmap.

ASCII encoding, start with space (0x20)

take a font pixmap (1bpp), with name fontname_8x12.png

output the font as a .bin as used in blt_surface :
- u8 w,h pixel width of a character
- 256 widths in pixels (as 4 bits : 128bytes)
- N characters in pixels made of (w+7)/8*h bytes (ie each line has a whole number of bytes)

"""

import sys
from PIL import Image
import itertools

sys.argv.append('../../textmode/fonts/font8x12.png')
size_y = 12
size_x = 8

img =Image.open(sys.argv[1]).convert('P')

letters = []
for y in range(img.size[1]//size_y) :
    for x in range(img.size[0]//size_x) :
        block = tuple(img.crop((x*size_x,y*size_y,(x+1)*size_x,(y+1)*size_y)).getdata())
        # put it in lines
        bitlines = [block[l*size_x:l*size_x+size_x] for l in range(size_y)]
        # get left-right zero
        for right in range(size_x-1,0,-1) :
            if any(bitlines[y][right]==1 for y in range(size_y)) :
                break
        left=0
        for left in range(0,right) :
            if any(bitlines[y][left]==1 for y in range(size_y)) :
                break
        letters.append((bitlines,left,right-left+1))
        for l in bitlines : print (''.join('.#'[b] for b in l))
        print (left,right-left+1)

        bytes = ["0x%02x"%int("".join("01"[i] for i in block[l*size_x:(l+1)*size_x]),2) for l in range(size_y)]

#        print ("{",", ".join(bytes),"}, //",y*16+x,repr(chr(y*16+x)))

# now output data as bytes (on stdout)

