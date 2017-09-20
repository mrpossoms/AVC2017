import tensorflow as tf
import struct
import sys
import subprocess
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

    def reset(self):
        self.index = 0
        self.file.seek(self.data_start, 0);

    def size(self):
        last_pos = self.file.tell()
        self.file.seek(0, 2)
        size = self.file.tell() // sample_size()
        self.file.seek(last_pos, 0)
        return size

    def decode_sample(self):
        start = self.file.tell()
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
        stop = self.file.tell()

        print(stop - start)
        state = (np.frombuffer(luma, dtype=np.uint8).reshape(np.prod(self.shape)) / 255.0)

        return np.asarray(state , dtype=np.float32).reshape([1, np.prod(self.shape)]), \
               np.asarray(action_vector, dtype=np.float32).reshape([1, 22])

    def next_batch(self, size):
        return None
        #return np.asarray(images), np.asarray(labels)


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

        correct_prediction = tf.equal(tf.argmax(self.h, 1), tf.argmax(self.y_))
        self.prediction_accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

    def train(self, x, y_):
        self.training_step.run(feed_dict={"X:0": x, "y_labels:0": y_})

    def predict(self, x):
        return self.h.eval(feed_dict={x: x})

    def accuracy(self, x, y_):
        self.prediction_accuracy.eval(feed_dict={"X": x, "y_labels": y_})



def main(args):
    net = AvoiderNet()

    ts = BlobTrainingSet(path="s0")
    ts.reset()
    ts.decode_sample()
    ts.decode_sample()
    ts.decode_sample()

    with tf.Session() as session:
        session.run(tf.global_variables_initializer())
        x, y_ = ts.decode_sample()
        net.train(x, y_)


    print(x, y_)

if __name__ == '__main__':
    main(sys.argv)