#!/usr/bin/env python3

"""
TSET file format : 
    u8 tilesize (8,16)
    u8 datacode : 0 = u16, 1=u8, 2=couples references in palette
    u16 nb of tiles
    (optional) : couples palette as u16
    u8 or u16 data[]    
"""

# fixme export tile names (type)

import argparse
from utils import *
import sys
import struct
import array
import xml.etree.ElementTree as ET

from PIL import Image

TILESIZES = (8,16)

def export_tset(name, tilesize, img, palette_type, maxtile) : 
    "tsx export to tileset tset file"
    print(' - writing',name+'.tset', file=sys.stderr)

    src = Image.open(img).convert('RGBA')
    w,h = src.size

    nbtiles = min(maxtile, (w//tilesize)*(h//tilesize))

    if palette_type == None : #u16
        datacode = DATA_u16
        data = tuple(rgba2u16(*c) for c in src.getdata())
    elif palette_type == 'COUPLES' : 
        raise NotImplemented("couples")
    else : 
        if palette_type=='MICRO' : 
            palette = gen_micro_pal()
        else :
            palette = Image.open(palette_type)
        datacode = DATA_u8


        new = src.convert('RGB').quantize(palette=palette) 
        data = new.getdata()

    pixdata = array.array('HBB'[datacode], data)

    with open(name+'.tset','wb') as of:
        # header
        of.write(struct.pack('BBH',tilesize,datacode,nbtiles))

        # tiles
        t_w = w//tilesize
        for tile in range(nbtiles) : 
            tile_x, tile_y = tile%t_w, tile//t_w
            for row in range(tilesize) :
                idx = (tile_y*tilesize+row)*w + tile_x*tilesize
                pixdata[idx:idx+tilesize].tofile(of)

# --- Main : commandline parsing

if __name__=='__main__' : 
    parser = argparse.ArgumentParser(description='Process png or tsx to generate tset file.')
    parser.add_argument('file', metavar='file',help='input file (tsx or png)') 
    parser.add_argument('-p','--palette', help='palette name/file. Can use a .png file (255 colors max + transp) or MICRO, or COUPLES')
    parser.add_argument('-s','--size', help='(for png) size in pixels of a tile', choices=(8,16),type=int)
    parser.add_argument('-m','--max_tiles', help='export at most max_tiles tiles even if tileset is bigger.',type=int)

    args = parser.parse_args()

    def usage(str) : 
        print("Usage error :", str, file=sys.stderr)
        sys.exit(2)

    file_name,file_ext = args.file.rsplit('.',1)

    # dispatch from first file type
    if file_ext == 'png' : 
        if args.size == None : usage ('must specify tilesize when input is png')
        name=file_name             
        tilesize = int(args.size)
        img = args.file
        maxtile=9999999 # not limited yet

    elif file_ext == 'tsx' : 
        if args.size : usage('cannot specify size for tsx files')
        ts=ET.parse(args.file).getroot()
        name = ts.get('name')
        img = abspath(args.file, ts.find("image").get("source"))
        tilesize = int(ts.get("tilewidth"))
        maxtile = int(ts.get('tilecount'))

        assert tilesize == int(ts.get("tileheight")),'only square tiles'
    else : 
        usage('unknown input file type')
    
    assert tilesize in TILESIZES, "tiles sizes must be 8 or 16 "
    if args.max_tiles : 
        maxtile = min(maxtile, args.max_tiles)
    export_tset(file_name, tilesize, img, args.palette, maxtile) 
