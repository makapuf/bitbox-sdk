#!/usr/bin/env python
"""
Simple TMX to TMAP export

.map format is : 
    - u16 header = 0xb17b
    - u16 map_w, 
    - u16 map_h, 
    - u8 map_codec 0=u8, 1=u16, ..
    - u8 nb of layers, 

    - levels : N times layers

    - objects : 
        s16 x
        s16 y
        u8 object_id 
        u8 value  (int(name) if name is an int or tile.property_value[name])
        u8 reserved[2]

        record oid=255 separate object groups
        record oid=255 value=255 ends the file

    + references list of files as a .tmx file

    no tileset export, always use .tsx files.

"""

import sys
import os
import argparse
import array
import struct
import xml.etree.ElementTree as ET

from utils import abspath

MAP_FORMATS = 'u16','u8'

def tid2ts(tmx,id, basefile) : 
    "for a given tid in tmx, find the tileset + local tid type"
    for ts in tmx.findall('tileset'):
        if int(ts.get('firstgid'))>id:  
            break
        last_ts=ts

    rid=id-int(last_ts.get('firstgid'))
    src=last_ts.get('source')
    if src : 
        ts = ET.parse(abspath(basefile,src)).getroot()
    # fixme else : included in tmx
    tile = ts.find('tile[@id="%d"]'%rid)
    return ts.get('name'), tile.get('type') if tile is not None else None

def out_objects(tmap, basefile, of=None) : 
    """count objects, find ids
    save values to file of.
    returns 
        - [('layername1',index_first_object), ('layername2',idx), ...] 
        - [(ts,type),(ts,type),...] uniques - to spr files references
    """
    unique_oids=[]
    objgroups = []
    index=0
    for objgroup in tmap.findall('objectgroup'): 
        name=objgroup.get("name")
        if name[0]=='_' : continue # skip
        objgroups.append((name,index))
        
        if of : print "#define objgroup_%s_%s %d"%(tmap_name, name, index)
        for obj in objgroup.findall('object'):
           
            oid = int(obj.get('gid'))
            if oid not in unique_oids : 
                unique_oids.append(oid)
            uid=unique_oids.index(oid)
            nm = obj.get('name')

            x = int(obj.get('x'))
            y = int(obj.get('y'))
            if nm and nm.startswith('_') : continue
            v = int(nm) if nm is not None and nm.isdigit() else 0
            # fixme values from names / other properties ?

            if of : of.write(struct.pack('2h4B',x,y,uid,v,0,0)) ; index +=1
            # verify ids from all tsx + local, find sprite .. 
        if of : of.write(struct.pack('2h4B',0,0,255,0,0,0)) ; index +=1
    if of : 
        of.seek(-8,2)
        of.write(struct.pack('2h4B',0,0,255,255,255,255)) ; index +=1

    return objgroups, [tid2ts(tmap,oid,basefile) for oid in unique_oids]


def out_layers(tmap,of=None,codec='u8') : 
    index=0
    mw = int(tmap.get('width'))
    mh = int(tmap.get('height'))
    nb_layers = sum(1 for x in tmap.findall('layer') if not(x.get('name').startswith('_')))
    if of : 
        array.array('H',(
            0xb17b,
            mw,mh,
            MAP_FORMATS.index(codec)<<8 | nb_layers),            
        ).tofile(of)

    min_indices=99999999
    max_indices=-1

    for layer in tmap.findall('layer') :
        name=layer.get("name")
        if name[0]=='_' : continue # skip
        
        lw = int(layer.get("width"))
        lh = int(layer.get("height"))
        out_code='H' if codec=='u16' else 'B'

        data = layer.find("data")
        if data.get('encoding')=='csv' :
            indices = tuple(int(s) for s in data.text.replace("\n",'').split(','))                
        elif data.get('encoding')=='base64' and data.get('compression')=='zlib' :
            indices = array.array('I',data.text.decode('base64').decode('zlib'))
        else :
            raise ValueError,'Unsupported layer encoding :'+data.get('encoding')


        if codec=='u8' and max(indices)>=256 : 
            raise ValueError,"size of index type too small : %d tiles used"%max(indices)
        assert len(indices) == lw*lh, "not enough or too much data : %d != %d"%(lw*lh, len(tidx))

        # output data to binary
        if of :
            print "#define layer_%s_%s %d"%(tmap_name, name, index)
            array.array(out_code,indices).tofile(of)
        index += 1

        # update lowest / highest id so far
        min_indices = min(min_indices, min(x for x in indices if x)) # not zero : no tile
        max_indices = max(max_indices, max(indices))

    return max_indices, min_indices

def read_tmap(file) : 
    print "showing info about map",file
    f = open(file,'rb')

    h_head,h_w,h_h,h_layers,h_format = struct.unpack('HHHBB', f.read(8))
    print "Header     : 0x%x"%h_head
    print "Map Size   : %dx%d - %d"%(h_w,h_h,h_w*h_h)
    print "Format     : %d - %s"%(h_format, MAP_FORMATS[h_format])
    print "Map Layers : %d"%h_layers

    print ' ---'

    for i in range(h_layers) :
        arr=array.array('BH'[h_format-1])
        arr.fromfile(f,h_h*h_w)
        print "layer %d : "%i,arr[:20],'...'
    print ' ---'
    group = 0
    while True : 
        x,y,uid,v,v2,v3 = struct.unpack('2h4B',f.read(8))
        if uid==255 : 
            group+=1
            if v==255 : break # eof marker
        else : 
            print "object group:%d x:%3d y:%3d oid:%d value:%d"%(group,x,y,uid,v)
    print ' ---'


if __name__=='__main__' : 
    parser = argparse.ArgumentParser(description='Process files for bitbox graphical library.')
    parser.add_argument('filename', help='input file (tmx)') 
    parser.add_argument('-f','--format',  help='format of the tilemap index (u8, u16)', default='u8')

    args = parser.parse_args()

    if args.filename.rsplit('.',1)[1]=='map' :
        read_tmap(args.filename)
        sys.exit(0)

    infile = open(args.filename,'r')

    def usage(str) : 
        print >>sys.stderr, " Usage error :", str
        sys.exit(2)

    if args.format not in MAP_FORMATS : usage('format not in '+','.join(MAP_FORMATS))


    tmap = ET.parse(args.filename).getroot()
    tmap_name = args.filename.rsplit('.',1)[0].rsplit('/',1)[-1]
    of = open(tmap_name+'.map','wb')

    print >>sys.stderr, " Generating tilemap file %s from %s using format %s"%(of.name, args.filename, args.format)

    m_idx,M_idx = out_layers (tmap, of, args.format)

    # fixme: .mk ?

    # write file references
    # reference (unique) to tileset
    if m_idx>=0 : # any layer defined  
        tsA, _ = tid2ts(tmap,m_idx,args.filename)
        tsB, _ = tid2ts(tmap,M_idx,args.filename)
        if tsA != tsB : 
            # not really an error, just an info
            print >>sys.stderr, ' stderr from several tilesets (%s, %s) are used for this tilemap. only first is included.'%(tsA,tsB)
        print "#define %s_MAP  \"%s_map\""%(tmap_name, tmap_name)
        print "#define %s_TSET \"%s_tset\""%(tmap_name, tsA)
    print

    if objgroups : 
        print '#define %s_OBJECT_GROUPS \\'%tmap_name
        objgroups, unique_states = out_objects(tmap, args.filename, of)
        for name, index in objgroups : 
            print "    X(%s,%s) \\"%(name,index)
    if unique_states : 
        print '#define %s_OBJECTS \\'%tmap_name
        for ts,typ in unique_states : 
            if typ==None : usage('Referenced tile in %s has no type !'%ts)
            print "    X(%s,%s) \\"%(ts,typ)
    print
