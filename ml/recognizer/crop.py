#!/Users/kirk/.pyenv/shims/python3
from PIL import Image
import random
import string
import sys
import io
import os
import time

random.seed(time.time())

def name():
    return ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(10))

str_buf = ''
#for unit in sys.stdin.read(1):
#    print(unit)
    #str_buf += line

#buf = io.BytesIO(sys.stdin.buffer)


os.makedirs('imgs/{}'.format(sys.argv[1]), exist_ok=True)
i = 0

while True:
    line = sys.stdin.readline()
    path = line.rstrip()

    if len(path) == 0:
        break

    img = Image.open(path)

    img_w, img_h = (img.width, img.height)
    print("\n%dx%d" % (img.width, img.height))

    img.save("before.png", "PNG");

    if sys.argv[2] == 'no-sky':
        img_h //= 2

    w, h = 32, 32

    i += 1

    for _ in range(12):
        x_range = img.width - w
        y_range = img_h - h
        x, y = int(random.random() * x_range), int(random.random() * y_range) + (img.height - img_h)
        bounds = [x, y, x + w, y + h]
        img.crop(bounds).save('imgs/%s/%s' % (sys.argv[1], name()), 'PNG')

    os.unlink(path)
