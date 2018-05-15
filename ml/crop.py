#!/Users/kirk/.pyenv/shims/python3
from PIL import Image
import random
import string
import sys
import os
import time
import sym


random.seed(time.time())


def name():
    return ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(10))

str_buf = ''


symli = sym.Li("Slices larger images up into smaller images")
symli.optional('no-sky', 'This flag will clip the top half of the image off')\
     .required('class', 'Specifies the directory to save into. i.e. imgs/[class]/')


os.makedirs('{}'.format(symli['class']), exist_ok=True)
i = 0

while True:
    line = sys.stdin.readline()
    path = line.rstrip()

    if len(path) == 0:
        break

    img = Image.open(path)

    img_w, img_h = (img.width, img.height)

    if symli['no-sky']:
        img_h //= 2

    w, h = 16, 16
    i += 1

    for _ in range(12):
        x_range = img.width - w
        y_range = img_h - h
        x, y = int(random.random() * x_range), int(random.random() * y_range) + (img.height - img_h)
        bounds = [x, y, x + w, y + h]
        img.crop(bounds).save('%s/%s' % (symli['class'], name()), 'PNG')

    os.unlink(path)
