from sklearn import neural_network as nn
import numpy as np
from PIL import Image
from random import shuffle
import os
import time


def filenames_labels():
    files_labels = []

    for positive in os.listdir('imgs/hay'):
        files_labels += [('imgs/hay/' + positive, 1)]

    for negative in os.listdir('imgs/not-hay'):
        files_labels += [('imgs/not-hay/' + negative, 0)]

    shuffle(files_labels)

    return files_labels

def show_example(X, i, title=None):
    test = Image.fromarray(((X[i] + 0.5) * 256).reshape((32, 32, 3)).astype(np.uint8))
    test.show(title=title)

def minibatch(files_labels, index, size=100):
    X, Y = [], []
    for file, label in files_labels[index * size: index * size + size]:
        img = Image.open(file).convert('RGB')
        img_array = np.array(img)

        assert(img_array.size == 3 * 32**2)

        X += [img_array.flatten()]
        Y += [[label]]

    return (np.array(X).reshape((size, 3072)) / 256.0 - 0.5), np.array(Y).reshape((size, 1))

ts = filenames_labels()
m_b = len(ts) // 100

model = nn.MLPClassifier(learning_rate_init=0.0001, solver='adam', max_iter=2, warm_start=True)

for i in range(m_b):
    X, Y = minibatch(ts, i)
    model.fit(X, Y)

    if i % 10 == 0:
        print(model.score(X, Y))

ts_X, ts_Y = minibatch(ts, 20, size=10)
print(model.score(ts_X, ts_Y))

y_ = model.predict(ts_X)

print(y_)
print(ts_Y.flatten())

shown_positive, shown_negative = False, False
for i in range(y_.size):
    if y_[i] == 0 and not shown_negative:
        show_example(ts_X, i)
        time.sleep(1)
        print("Wrong was %d" % y_[i])
        time.sleep(1)
        shown_negative = True

    if y_[i] == 1 and not shown_positive:
        show_example(ts_X, i)
        time.sleep(1)
        print("Right was %d" % y_[i])
        time.sleep(1)
        shown_positive = True


    if shown_positive and shown_negative:
        break
