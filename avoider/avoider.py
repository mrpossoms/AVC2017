import tensorflow as tf
import struct
import sys
from fibers.fibers import *


class BlobTrainingSet():
    def __init__(self, path, shape=[160, 120]):
        self.index = 0
        self.file = open(path, mode='rb')
        self.shape = shape

    def reset(self):
        self.index = 0
        self.file.seek(0);

    def size(self):
        last_pos = self.file.tell()
        self.file.seek(0, 2)
        size = self.file.tell() // (4 + np.prod(self.shape))
        self.file.seek(last_pos, 0)
        return size

    def next_batch(self, size):
        images, labels = [], []


        # coord = tf.train.Coordinator()
        # threads = tf.train.start_queue_runners(coord=coord)

        for _ in range(size):
            buf = self.file.read(np.prod(self.shape))
            tag = struct.unpack('I', self.file.read(4))[0]

            assert tag is 0 or tag is 1

            images += [ np.frombuffer(buf, dtype=np.uint8).reshape(np.prod(self.shape)) / 255.0 ]
            # Image.frombytes('L', (128, 128), buf).show()
            if tag == 1:
                labels += [1.0]
            else:
                labels += [-1.0]

        self.index += size
        # coord.request_stop()
        # coord.join(threads)

        batch = np.asarray(images), np.asarray(labels).reshape([len(labels), 1])
        return batch
        #return np.asarray(images), np.asarray(labels)


class AvoiderNet:
    def __init__(self):
        self.x = x = tf.placeholder(tf.float32, shape=[None, 19200])
        self.y_ = tf.placeholder(tf.float32, shape=[None, 22])

        self.x_image = x_image = tf.reshape(x, [-1, 160, 120, 1])
        self.conv_layers = (image_input(x_image, [5, 5, 1])
                            .pool(2)
                            .to_conv(filter=[5, 5, 8])
                            .pool(2))

        self.h = self.conv_layers.to_fc(1024).output_vector(22)
        cross_entropy = tf.reduce_mean(tf.nn.softmax_cross_entropy_with_logits(labels=self.y_, logits=self.h))
        self.training_step = tf.train.AdamOptimizer(1E-4).minimize(cross_entropy)

        correct_prediction = tf.equal(tf.argmax(self.h, 1), tf.argmax(self.y_))
        self.prediction_accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

    def train(self, x, y_):
        self.training_step.run(feed_dict={x: x, y_: y_})

    def predict(self, x):
        return self.h.eval(feed_dict={x: x})

    def accuracy(self, x, y_):
        self.prediction_accuracy.eval(feed_dict={x: x, y_: y_})



def main(args):
    with tf.Session() as session:
        net = AvoiderNet()


if __name__ == '__main__':
    main(sys.argv)