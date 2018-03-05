#!/usr/bin/python3

from PIL import Image
import sys
import io

str_buf = ''
#for unit in sys.stdin.read(1):
#    print(unit)
    #str_buf += line

#buf = io.BytesIO(sys.stdin.buffer)


img = Image.open(sys.stdin.buffer)
img.crop([0, 0, 64, 64]).save(sys.argv[1], 'PNG')
