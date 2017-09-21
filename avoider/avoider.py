import tensorflow as tf
import struct
import sys
import os
import subprocess
from random import shuffle
from fibers.fibers import *


FRAME_W=160
FRAME_H=120
LUMA_PIXELS = (FRAME_W * FRAME_H)
CHRO_PIXELS = (FRAME_W / 2 * FRAME_H)
SAMPLE_SIZE = None


class BlobTrainingSet():
    def __init__(self, path, shape=[160, 120, 1]):
        self.index = 0
        self.file = open(path, mode='rb')
        self.shape = shape

        self.magic = struct.unpack('Q', self.file.read(8))
        self.is_raw = self.file.read(8);
        self.data_start = self.file.tell()

        self.reset()



    def seek_sample(self, sample_index):
        self.file.seek(self.data_start + sample_size() * sample_index, 0)

    def reset(self):
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
        while(added < size and len(self.order) > 0):
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
        cross_entropy = tf.reduce_mean(tf.nn.softmax_cross_entropy_with_logits(labels=self.y_, logits=self.h))
        self.training_step = tf.train.AdamOptimizer(1E-4).minimize(cross_entropy)

        correct_prediction = tf.equal(tf.argmax(self.h, 1), tf.argmax(self.y_, 1))
        self.prediction_accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

    def train(self, x, y_):
        self.training_step.run(feed_dict={"X:0": x, "y_labels:0": y_})

    def predict(self, x):
        return self.h.eval(feed_dict={"X:0": x})

    def accuracy(self, x, y_):
        return self.prediction_accuracy.eval(feed_dict={"X:0": x, "y_labels:0": y_})



def main(args):
    model_path = "/tmp/avoider.ckpt"
    net = AvoiderNet()
    ts = BlobTrainingSet(path="s0")
    saver = tf.train.Saver()

    with tf.Session() as session:
        saver.restore(session, model_path)

        session.run(tf.global_variables_initializer())

        epoch = 0
        while(len(ts.order) > 0):
            x, y_ = ts.next_batch(50)
            net.train(x, y_)
            epoch += 1

            print(len(ts.order))
            if epoch % 10 == 0:
                saver.save(session, model_path)

        ts.reset()
        x, y_ = ts.next_batch(1000)
        print(net.accuracy(x, y_))

        ts.reset()
        x, y_ = ts.next_batch(1)
        print(net.predict(x))
        print(y_)

if __name__ == '__main__':
    main(sys.argv)