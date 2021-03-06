#!/usr/bin/env python3

'''
spr2png.py : simple decoder for spr format to single png vertical strip
    decode duplicated frames (which are deduplicated in format)
    you can set TYPECOLOR to True to replace each blit type with a color instead of original pixels

    data : red
    fill : blue
    copy : green
    skip : transparent

'''

"""
TODO
Add args : typecolor, debug, frame columns ... 
implement copy for u16
typecolor tints instead of replacing ?

"""

from PIL import Image
import struct 
from utils import u162rgba, u82rgba
import sys 

CODE_SKIP = 0
CODE_FILL = 1
CODE_DATA = 2
CODE_REF  = 3

DATACODE_STR = ['u16','u8','couples']

DATA_u16 = 0
DATA_u8  = 1
DATA_cpl = 2

DEBUG = False
TYPECOLOR = False # replace blits by a color for their type

class Sprite : 
    def __init__(self, f) : 
        self.f = open(f,'rb')
        self.palette = None
        self.parse_header()
        self.read_data = [self.read_data_u16,self.read_data_u8,self.read_data_cpl][self.datacode]

    def read_data_u16(self,nb) : 
        return [u162rgba(c) for c in struct.unpack('%dH'%nb, self.f.read(nb*2))]

    def read_data_u8(self,nb) : 
        return [u82rgba(c) for c in struct.unpack('%dB'%nb, self.f.read(nb))]

    def read_data_cpl(self,nb) : 
        data = ()
        for i in range((nb+1)//2) : 
            c = ord(self.f.read(1))
            data += self.palette[c]

        # adjust for last one if odd
        if nb%2 : 
            data = data[:-1]
        return data

    def parse_header(self) : 
        magic,self.width,self.height,self.nbframes, self.datacode = struct.unpack('HHHBB', self.f.read(8))
        assert magic == 0xb17b
        self.hitbox = struct.unpack ('4H',self.f.read(8))
        total_lines = self.height*self.nbframes
        self.lines_index = struct.unpack('%dH'%total_lines, self.f.read(total_lines*2))
        # now read palette if needed
        if self.datacode == DATA_cpl : 
            s= self.f.read(4)
            palette_len = struct.unpack('I',s)[0]
            palette = [u162rgba(x) for x in  struct.unpack('%dH'%palette_len*2,self.f.read(palette_len*4))]
            self.palette = [(palette[2*i],palette[2*i+1]) for i in range(palette_len)]

    def read_blit(self) : 
        c = ord(self.f.read(1))
        blit_code = c>>6
        blit_eol = c & 1<<5 == 32
        blit_len = c & 31
        # to optimize away if not needed (total width<32 par ex.)
        if blit_len==31 :
            while True : 
                c = ord(self.f.read(1))
                blit_len += c
                if c<255 : break

        yield blit_code, blit_len, blit_eol

    def unpack(self) : 
        self.img = Image.new('RGBA',(self.width,self.height*self.nbframes))
        data = self.img.load()
        file_start = self.f.tell()

        # frame by frame, seek to position first (duplicated frames !)
        for y, line_idx in enumerate(self.lines_index) : 
            x=0
            self.f.seek(file_start+line_idx)

            while True : # fill line
                bc,bl,beol = next(self.read_blit())
                if DEBUG : 
                    print (['skip','fill','data','copy'][bc],bl,'eol:', beol,'pos:',x,y)

                if bc==CODE_DATA : 
                    pixels = self.read_data(bl)
                    for i,color in enumerate(pixels) : 
                        if TYPECOLOR : color = (255,0,0,255)  
                        data[x+i,y] = color

                elif bc==CODE_FILL : 
                    if self.datacode==DATA_u16 : 
                        color = u162rgba(struct.unpack('H',self.f.read(2))[0])                    
                        if TYPECOLOR : color = (0,0,255,255)  
                        for i in range(bl) : data[x+i,y]=color
                    elif self.datacode == DATA_u8 : 
                        color = u82rgba(struct.unpack('B',self.f.read(1))[0])
                        if TYPECOLOR : color = (0,0,255,255)  
                        for i in range(bl) : data[x+i,y]=color
                    elif self.datacode == DATA_cpl : 
                        color = self.palette[ord(self.f.read(1))]
                        if TYPECOLOR : color = ((0,0,255,255),(0,0,200,255))

                        for i in range(0,bl-1,2) : 
                            data[x+i  ,y]=color[0]
                            data[x+i+1,y]=color[1]
                        if bl%2 : 
                            data[x+bl-1,y]=color[0]

                    else : 
                        raise ValueError('unknown datacode')

                elif bc == CODE_SKIP : 
                    pass

                elif bc == CODE_REF : 
                    # read back data index as u16
                    idx = struct.unpack('<H',self.f.read(2))[0]
                    oldpos = self.f.tell() # save for later
                    idx += 2 #adjust
                    self.f.seek(-idx,1)  # go to back reference
                    if self.datacode == DATA_cpl : 
                        pixels = ()
                        for i in range(bl//2) : 
                            c = ord(self.f.read(1))
                            pixels += self.palette[c]
                        if bl%2 : 
                            pixels += (self.palette[ord(self.f.read(1))][0],)
                    elif self.datacode == DATA_u8 :
                        pixels = [u82rgba(c) for c in self.f.read(bl)]
                    else : 
                        pixels = [(255,0,255,255)]*bl
                        # fixme implement it
                    self.f.seek(oldpos)  # get back to where we were

                    for i,color in enumerate(pixels) : 
                        if TYPECOLOR : color = (0,255,0,255)  
                        data[x+i,y] = color


                if beol :   
                    break
                else : 
                    x+=bl

    def __str__(self) :
        s= "%dx%d, %d frames, datacode:%d (%s)"%(self.width, self.height, self.nbframes, self.datacode, DATACODE_STR[self.datacode])
        s+= '\nhitbox : %d,%d-%d,%d'%self.hitbox
        if self.palette : s += "\n%d couples in palette"%len(self.palette)
        return s

if __name__=='__main__' : 
    s = Sprite(sys.argv[1]) 
    print (s)
    s.unpack()
    s.img.save(sys.argv[1]+'.png')

