import numpy as np

TRAINING_SET_BASE='ds/training/aug'
PATCH_SIDE = 12
PATCH_SIZE = PATCH_SIDE * PATCH_SIDE * 3


def array_from_file(path):
    from PIL import Image

    img = Image.open(path).convert('RGB')
    return np.array(img)[0:PATCH_SIDE,0:PATCH_SIDE,:]


# def process_example(x):
#     # mu = np.average(x, axis=(0, 1))
#     return (np.array((x).flatten(), dtype=np.float32) / 255.0) - 0.5


def minibatch(files_labels, index, size=100):
    X, Y = [], []

    index = index % (len(files_labels) - size)

    if size == -1:
        index = 0
        size = len(files_labels)

    for file, label in files_labels[index * size: index * size + size]:
        img_array = array_from_file(file)
        assert(img_array.size == PATCH_SIZE)

        X += [img_array]
        Y += [label]

    m = len(X)

    X = (np.array(X, dtype=np.float32).reshape((m, PATCH_SIZE)) / 255.0) - 0.5

    return X, np.array(Y, dtype=np.float32).reshape((m, 3))


def serialize_matrix(m, fp):
    """
    Writes a numpy array into fp in the simple format that
    libnn's nn_mat_load() function understands
    :param m: numpy matrix
    :param fp: file stream
    :return: void
    """
    import struct

    # write the header
    fp.write(struct.pack('b', len(m.shape)))
    for d in m.shape:
        fp.write(struct.pack('i', d))

    # followed by each element
    for e in m.flatten():
        fp.write(struct.pack('f', e))


def deserialize_matrix(fp):
    import struct

    def read(fmt):
        return struct.unpack(fmt, fp.read(struct.calcsize(fmt)))

    # read the shape of the matrix
    shape = [read('i')[0] for _ in range(read('b')[0])]

    # read the matrix elements, and reshape the matrix
    m = np.fromfile(fp, dtype=np.float32, count=np.prod(shape)).reshape(shape)

    return m


def filenames_labels():
    global TRAINING_SET_BASE
    classes = [0, 1, 2]
    files_labels = []

    import os
    import random

    for c in classes:
        for f in os.listdir(TRAINING_SET_BASE + '/' + str(c)):
            label = [0] * len(classes)
            label[c] = 1
            files_labels += [(TRAINING_SET_BASE + '/' + str(c) + '/' + f, label)]

    random.shuffle(files_labels)

    return files_labels


def print_stats(t_1, t):
    width = 20
    display_bar = [' '] * width

    acc_t_1, acc_t = t_1['accuracy'], t['accuracy']
    epoch, total = t['epoch'], t['epoch_total']
    acc_delta = acc_t - acc_t_1

    slope_i = int((width - 1) * acc_t)

    if acc_delta > 0.01:
        display_bar[slope_i] = '\\'
    elif acc_delta < -0.01:
        display_bar[slope_i] = '/'
    else:
        display_bar[slope_i] = '|'

    line = '%03d%% |' % (acc_t * 100) + ''.join(display_bar) + '| ep: %d/%d' % (epoch, total)
    print(line)
