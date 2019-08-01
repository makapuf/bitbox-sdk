#!/usr/bin/python3
"""make proportional fonts from a bitmap.

ASCII encoding, start with space (0x20) - 128 chars max, first being space

take a font pixmap (1bpp), with name fontname_8x12.png

output the font as a .bin as used in blt_surface :
- u8 h, bytes per line pixel width of a character
- 128 widths in pixels (u8)
- N characters in pixels made of (w+7)/8*h bytes (ie each line has a whole number of bytes)

"""

import sys
from PIL import Image
import itertools


DEBUG=False

def out_font(filename, outfile):
    img = Image.open(filename)
    data = img.load()

    # assert 4 colors, 0 being bg
    assert len(set(img.getdata())) <= 4, set(img.getdata())
    assert data[0, 0] == 0, "bg color must be zero, now it's %d"%data[0,0]
    height = img.size[1]-1 # last line can be used for continuity but is skipped.

    def column_empty(x):
        return all(data[x, y] == 0 for y in range(height+1)) # take last line into account

    letters = [tuple(0 for y in range(height))]  # Space
    for transp, group in itertools.groupby(range(img.size[0]), key=lambda x: column_empty(x)):
        if not transp:
            cols = list(group)
            xstart, xend = cols[0], cols[-1] + 1
            if xstart == xend:
                continue
            let_block = img.crop((xstart, 0, xend, height))
            pixels = tuple(let_block.getdata())
            assert len(pixels) == (xend - xstart) * height, \
                "%d != %d" % (len(pixels), (xend - xstart) * height)
            #let_block.save("let_%d.png"%xstart)
            letters.append(pixels)

    pixels_w = max(len(x) // height for x in letters)
    bpl = (pixels_w * 2 + 7) // 8

    print(len(letters), "letters, max pixels X :", pixels_w, ",bytes per line :", bpl,
          ', bytes per char: ', bpl * height, ', total: ', len(letters) * bpl * height)

    # output data

    # As N bytes per line, 2bpp pixels.
    with open(outfile, 'wb') as of:
        of.write(bytes((height, bpl)))
        of.write(bytes(len(l) // height for l in letters))
        of.write(bytes(128 - len(letters)))  # zero padding 128 letters

        for i,l in enumerate(letters):
            w = len(l) // height
            bts = []
            if DEBUG: print(i+32,chr(i+32))
            for y in range(height):
                line = l[w * y:w * (y + 1)]
                n = sum(p << (2 * i) for i, p in enumerate(line))
                if DEBUG: print (line,n)
                for _ in range(bpl):
                    bts.append(n & 0xff)
                    n >>= 8
                assert n == 0
            of.write(bytes(bts))
        print(of.tell(), 'bytes written to', outfile)

out_font(sys.argv[1], sys.argv[2])
