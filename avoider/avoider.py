#!/Users/kirk/.pyenv/shims/python
import tensorflow as tf
import struct
import sys
import getopt
import subprocess
from random import shuffle
from fibers.fibers import *


FRAME_W=160
FRAME_H=120
LUMA_PIXELS = (FRAME_W * FRAME_H)
CHRO_PIXELS = (FRAME_W / 2 * FRAME_H)
SAMPLE_SIZE = None


class BlobTrainingSet():
    def __init__(self, path=None, shape=[160, 120, 1], file=None):
        self.index = 0
        self.is_stream = False

        if file is not None:
            self.file = file
            self.is_stream = True
        elif path is not None:
            self.file = open(path, mode='rb')

        self.shape = shape

        self.magic = struct.unpack('Q', self.file.read(8))
        self.is_raw = struct.unpack('Q', self.file.read(8))

        print('magic: %d' % self.magic)
        print('raw: %d' % self.is_raw)

        if self.is_stream is False:
            self.data_start = self.file.tell()

        self.reset()

    def seek_sample(self, sample_index):
        if self.is_stream:
            self.file.seek(self.data_start + sample_size() * sample_index, 0)

    def reset(self):
        if self.is_stream:
            return

        self.order = list(range(0, self.size() - 1))
        shuffle(self.order)
        self.index = 0
        self.file.seek(self.data_start, 0);

    def size(self):
        last_pos = self.file.tell()
        self.file.seek(0, 2)
        size = self.file.tell() // sample_size()
        self.file.seek(last_pos, 0)
        return size

    def decode_sample(self):
        rot_rate = struct.unpack('hhh', self.file.read(6))
        acc = struct.unpack('hhh', self.file.read(6))
        vel = struct.unpack('f', self.file.read(4))
        distance = struct.unpack('I', self.file.read(4));
        heading = struct.unpack('fff', self.file.read(12));
        position = struct.unpack('fff', self.file.read(12));
        luma = self.file.read(np.prod(self.shape))

        chroma_shape = [self.shape[0] // 2, self.shape[1]]
        chroma = self.file.read(self.shape[0] // 2 * self.shape[1] * 2)

        action_vector = struct.unpack('ffffffffffffffffffffff', self.file.read(4 * 22))

        state = (np.frombuffer(luma, dtype=np.uint8).reshape(np.prod(self.shape)) / 255.0)

        return np.asarray(state , dtype=np.float32), \
               np.asarray(action_vector, dtype=np.float32)

    def next_batch(self, size):

        samples, labels = [], []
        added = 0
        while(added < size and (self.is_stream or len(self.order) > 0)):
            if not self.is_stream:
                i = self.order.pop()
                self.seek_sample(i)

            x, y = self.decode_sample()

            samples.append(x)
            labels.append(y)
            added += 1

        return np.asarray(samples).reshape([added, 19200]), np.asarray(labels).reshape([added, 22])


def sample_size():
    global SAMPLE_SIZE
    if SAMPLE_SIZE is None:
        SAMPLE_SIZE = int(subprocess.check_output(['structsize']))

    return SAMPLE_SIZE


class AvoiderNet:
    def __init__(self):
        self.x = x = tf.placeholder(tf.float32, shape=[None, 19200], name='X')
        self.y_ = tf.placeholder(tf.float32, shape=[None, 22], name='y_labels')

        self.x_image = x_image = tf.reshape(x, [-1, 160, 120, 1])
        self.conv_layers = (image_input(x_image, [160, 120, 1])
                            .pool(2)
                            .to_conv(filter=[5, 5, 8])
                            .pool(2)
                           )

        self.h = self.conv_layers.to_fc(1024).output_vector(22)
        self.cost = tf.reduce_mean(tf.pow(self.h - self.y_, 2))
        self.training_step = tf.train.AdamOptimizer(1E-4).minimize(self.cost)

        correct_prediction = self.cost < 1
        self.prediction_accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

    def train(self, x, y_):
        print(self.cost.eval(feed_dict={"X:0": x, "y_labels:0": y_}))
        self.training_step.run(feed_dict={"X:0": x, "y_labels:0": y_})

    def predict(self, x):
        return self.h.eval(feed_dict={"X:0": x})

    def accuracy(self, x, y_):
        return self.prediction_accuracy.eval(feed_dict={"X:0": x, "y_labels:0": y_})



def main(args):
    model_path = "/tmp/avoider.ckpt"

    opts, _ = getopt.getopt(args, 'sp')
    ts = None
    predict = False
    use_stdin = False

    for k, v in opts:
        if 's' in k:
            use_stdin = True

        if 'p' in k:
            predict = True

    net = AvoiderNet()
    saver = tf.train.Saver()

    if use_stdin:
        ts = BlobTrainingSet(file=sys.stdin.buffer)
    else:
        ts = BlobTrainingSet(path="s0")

    with tf.Session() as session:
        if predict:
            print('Predicting output')
            try:
                saver.restore(session, model_path)
            except tf.errors.NotFoundError:
                print('Failed to load')
        else:
            print('Training')

        session.run(tf.global_variables_initializer())

        if predict:
            while True:
                x, y_ = ts.next_batch(1)

                h = net.predict(x).reshape([22])
                i = 0

                print('-------------------------------')
                for f in h:
                    i += 1
                    if i < 7: continue
                    str = ''
                    for _ in range(0, int(f * 10)):
                        str += '#'

                    print(str)

        else:
            for epoch in range(1, 50):
                ts.reset()
                while len(ts.order) > 0:
                    x, y_ = ts.next_batch(1000)
                    net.train(x, y_)

                    print(len(ts.order))
                    if epoch % 10 == 0:
                        saver.save(session, model_path)

                ts.reset()
                x, y_ = ts.next_batch(1000)
                print('accuracy: %f' % net.accuracy(x, y_))

            ts.reset()
            x, y_ = ts.next_batch(1000)
            print('accuracy: %f' % net.accuracy(x, y_))

            ts.reset()
            x, y_ = ts.next_batch(1)
            h = net.predict(x)
            print("h: %s" % h)
            print("y_: %s" % y_)

            avg = np.average((h - y_), 1)

            print(avg)

if __name__ == '__main__':
    main(sys.argv[1:])