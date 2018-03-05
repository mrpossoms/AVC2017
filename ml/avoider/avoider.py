#!/usr/bin/python
import tensorflow as tf
import struct
import sys
import getopt
import subprocess
from random import shuffle
from fibers import *
from PIL import Image


FRAME_W=160
FRAME_H=120
LUMA_PIXELS = (FRAME_W * FRAME_H)
CHRO_PIXELS = (FRAME_W / 2 * FRAME_H)
SAMPLE_SIZE = None


class BlobTrainingSet():
    def __init__(self, path=None, shape=[160, 120, 1], file=None):
        self.index = 0
        self.is_stream = False
        self.saved_one = False

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
        sample_index *= 3

        if self.is_stream is not True:
            self.file.seek(self.data_start + sample_size() * sample_index, 0)

    def reset(self):
        if self.is_stream:
            return

        example_count = (self.size() - 1) // 3

        self.order = list(range(0, example_count))
        shuffle(self.order)
        self.index = 0
        self.file.seek(self.data_start, 0)

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
        distance = struct.unpack('I', self.file.read(4))
        heading = struct.unpack('fff', self.file.read(12))
        position = struct.unpack('fff', self.file.read(12))
        luma = self.file.read(np.prod(self.shape))
        chroma = self.file.read(self.shape[0] // 2 * self.shape[1] * 2)

        throttle = struct.unpack('fffffff', self.file.read(4 * 7))
        steering = struct.unpack('fffffffffffffff', self.file.read(4 * 15))

        # yuv2 = []
        # for yi in range(0, self.shape[1]):
        #     for xi in range(0, self.shape[0]):
        #         i = yi * self.shape[0] + xi;
        #         j = yi * (self.shape[0] >> 1) + (xi >> 1)
        #
        #         yuv2 += [luma[i], chroma[(j << 1) + 0] , chroma[(j << 1) + 1]]

        #bitmap = np.asarray(luma , dtype=np.uint8)
        bitmap = np.frombuffer(luma, dtype=np.uint8)

        if not self.saved_one:
            cube = bitmap.reshape([120, 160])
            Image.fromarray(cube, 'L').save('./last.png')
            self.saved_one = True

        state = bitmap.reshape(np.prod([120, 160, 1])) / 255.0

        return state, \
               np.asarray(steering, dtype=np.float32)

    def next_batch(self, size):

        samples, labels = [], []
        added = 0
        while(added < size and (self.is_stream or len(self.order) > 0)):
            if not self.is_stream:
                i = self.order.pop()
                self.seek_sample(i)

            x = []
            y = None

            for _ in range(0, 3):
                x_, y = self.decode_sample()
                x += [x_]

            samples.append(np.asarray(x, dtype=np.float32))
            labels.append(y)
            added += 1

        return np.asarray(samples).reshape([added, 57600]), np.asarray(labels).reshape([added, 15])


def sample_size():
    global SAMPLE_SIZE
    if SAMPLE_SIZE is None:
        SAMPLE_SIZE = int(subprocess.check_output(['structsize']))

    return SAMPLE_SIZE


class AvoiderNet:
    def __init__(self, output_size):
        self.x = x = tf.placeholder(tf.float32, shape=[None, 57600], name='X')
        self.y_ = tf.placeholder(tf.float32, shape=[None, output_size], name='y_labels')

        self.x_image = x_image = tf.reshape(x, [-1, 120, 160, 3])
        self.conv_layers = (image_input(x_image, [120, 160, 3])
                            .pool(2)
                            .to_conv(filter=[5, 5, 32])
                            .pool(2)
                           )

        self.output = self.conv_layers.to_fc(1024).output_vector(output_size)
        self.h = self.output.tensor
        self.cost = tf.reduce_mean(tf.nn.softmax_cross_entropy_with_logits(labels=self.y_, logits=self.h))
        self.training_step = tf.train.AdamOptimizer(1E-4).minimize(self.cost)

        correct_prediction = tf.equal(tf.argmax(self.y_, 1), tf.argmax(self.h, 1))
        self.prediction_accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

    def saver_dict(self):
        return self.output.saver_dict()

    def train(self, x, y_):
        self.training_step.run(feed_dict={"X:0": x, "y_labels:0": y_})

    def predict(self, x):
        return self.h.eval(feed_dict={"X:0": x})

    def accuracy(self, x, y_):
        return self.prediction_accuracy.eval(feed_dict={"X:0": x, "y_labels:0": y_})



def main(args):
    model_path = "/tmp/avoider.ckpt"

    opts, _ = getopt.getopt(args, 'spl')
    ts = None
    predict = False
    use_stdin = False
    should_load = False

    for k, v in opts:
        if 's' in k:
            use_stdin = True

        if 'p' in k:
            predict = True

        if 'l' in k:
            should_load = True

    net = AvoiderNet(output_size=15)
    save_dic = net.saver_dict()
    saver = tf.train.Saver(save_dic)

    if use_stdin:
        ts = BlobTrainingSet(file=sys.stdin.buffer)
    else:
        ts = BlobTrainingSet(path="/var/AVC2017/avoider/s0")

    with tf.Session() as session:
        loaded = False
        session.run(tf.global_variables_initializer())

        if predict or should_load:
            try:
                print('Restoring session')
                saver.restore(session, model_path)
                loaded = True
            except tf.errors.NotFoundError:
                print('Failed to load')
        else:
            print('Training')

        if predict:
            print('Predicting output')
            while True:
                x, y_ = ts.next_batch(1)

                h = net.predict(x).reshape([15])
                i = 0

                print('-------------------------------')
                for f in h:
                    i += 1
                    if i == 7:
                        print('>>>')
                    str = ''
                    for _ in range(0, int(f * 10)):
                        str += '#'

                    print(str)

        else:
            for epoch in range(1, 200):
                ts.reset()
                while len(ts.order) > 0:
                    x, y_ = ts.next_batch(1000)
                    net.train(x, y_)

                    #if epoch % 10 == 0:
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
