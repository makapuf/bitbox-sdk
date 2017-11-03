"""make proportional fonts from a bitmap.
ASCII encoding, start with space (0x20)

- image must have one line
- first pixel found is bg, all others are fg
- first line is for widths : separate non-bg with multiple fg pixels fonts one fg pixel separate each letter
 example (space, D character) : 

--x----x 
--------
---xxx--
---x--x-
---x--x-
---xxx--
--------

outputs the font as a bitmap or a C file.
"""

import sys
from PIL import Image
import itertools 

sys.argv.append('font.png')

img =Image.open(sys.argv[1]).convert('P')
data=img.load()

bg = data[0,0]
# gets font height

grouped = itertools.groupby((data[x,0] for x in range(img.size[0])), key=lambda x:x==bg) 
letters = []
pos = 0
for i,(b,c) in enumerate(grouped) : 
	n = sum(1 for _ in c)
	if b : 
		#print i/2,pos,n
		pixels = []
		for y in range(1,img.size[1]) :
			row=tuple( 0 if data[pos+x,y]==bg else 1 for x in range(n) )
			pixels.append(row)
		letters.append(pixels)
	pos +=n

for row in letters[ord('[')-32] : 
	print ''.join('.#'[x] for x in row)

