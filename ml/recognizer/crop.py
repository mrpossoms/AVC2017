#!/usr/bin/python3

from PIL import Image
import random
import string
import sys
import io

def name():
    return ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(10))

str_buf = ''
#for unit in sys.stdin.read(1):
#    print(unit)
    #str_buf += line

#buf = io.BytesIO(sys.stdin.buffer)



img = Image.open(sys.stdin.buffer)

w, h = 64, 64

for _ in range(6):
    x_range = img.width - w
    y_range = img.height - h
    x, y = int(random.random() * x_range), int(random.random() * y_range)
    img.crop([x, y, x + 64, y + 64]).save('%s%s' % (sys.argv[1], name()), 'PNG')
