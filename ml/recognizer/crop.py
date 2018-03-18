#!/Users/kirk/.pyenv/shims/python3
from PIL import Image
import random
import string
import sys
import io
import os
import time

random.seed(time.time())

print(">>>>>")

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

    print('[[' + path + ']]')
    img = Image.open(path)

    w, h = 64, 64

    i += 1

    for _ in range(6):
        x_range = img.width - w
        y_range = img.height - h
        x, y = int(random.random() * x_range), int(random.random() * y_range)
        img.crop([x, y, x + 64, y + 64]).save('imgs/%s/%s' % (sys.argv[1], name()), 'PNG')
