from sklearn import neural_network as nn
import numpy as np
from PIL import Image
from random import shuffle
import os
import time

def activation_map(model, img_path):
    img = Image.open(img_path).convert('RGB')
    img_array = np.array(img)


def filenames_labels():
    files_labels = []

    for positive in os.listdir('imgs/hay'):
        files_labels += [('imgs/hay/' + positive, 1)]

    for negative in os.listdir('imgs/not-hay'):
        files_labels += [('imgs/not-hay/' + negative, 0)]

    shuffle(files_labels)

    return files_labels


def real_test_filenames_labels():
    return [
        ('imgs/real/real0', 1),
        ('imgs/real/real1', 1)
    ]


def array_from_file(path):
    img = Image.open(path).convert('RGB')
    return np.array(img)


def show_example(X, i, title=None):
    test = Image.fromarray(((X[i] + 0.5) * 256).reshape((32, 32, 3)).astype(np.uint8))
    test.show(title=title)


def minibatch(files_labels, index, size=100):
    X, Y = [], []

    for file, label in files_labels[index * size: index * size + size]:
        img_array = array_from_file(file)
        assert(img_array.size == 3 * 32**2)

        X += [img_array.flatten()]
        Y += [[label]]

    m = len(X)

    return (np.array(X).reshape((m, 3072)) / 256.0 - 0.5), np.array(Y).reshape((m, 1))


# Get the set of all the labels and file paths, pre shuffled
full_set = filenames_labels()

# find total number of samples, compute count of complete batches
m = len(full_set)
batches = m // 100

# Split in to a training and test set
ts_batches = int(np.floor(batches * 0.75)) - 1
training_set = full_set[0:ts_batches * 100]
test_set = full_set[ts_batches * 100:-1]

model = nn.MLPClassifier(hidden_layer_sizes=[256, 256, 256],
                         learning_rate_init=0.0001,
                         alpha=0.001,
                         solver='adam',
                         max_iter=2,
                         warm_start=True)

for _ in range(2):
    for i in range(ts_batches):
        X, Y = minibatch(training_set, i)
        model.fit(X, Y)

        if i % 10 == 0:
            print(model.score(X, Y))

ts_X, ts_Y = minibatch(test_set, 0, size=100)
print('Test set score: %f' % model.score(ts_X, ts_Y))

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

X, Y = minibatch(real_test_filenames_labels(), 0, size=2)
print('Real set score: %f' % model.score(X, Y))
print(model.predict(X))
