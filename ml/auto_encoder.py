"""Tutorial on how to create a convolutional autoencoder w/ Tensorflow.

Parag K. Mital, Jan 2016
"""
import tensorflow as tf
import numpy as np
import math
import struct
import os
# from libs.utils import corrupt

FRAME_W = 160
FRAME_H = 120
VIEW_PIXELS = (FRAME_W * FRAME_H)
LUMA_PIXELS = (FRAME_W * FRAME_H)
CHRO_PIXELS = (FRAME_W / 2 * FRAME_H)


def clamp(n, min_, max_):
    return int(max(min_, min(max_, n)))


class AvcRawState():
    @staticmethod
    def formats():
        return [
            # typedef struct {
            #     uint64_t magic;
            #     payload_type_t type;
            # } dataset_hdr_t;
            "QI",
            # int16_t  rot_rate[3];
            # int16_t  acc[3];
            # float    vel;
            # float    distance;
            # vec3     heading;
            # vec3     position;
            "hhhhhhffffffff",
            # uint8_t luma[LUMA_PIXELS];
            "B" * int(LUMA_PIXELS),
            # chroma_t chroma[CHRO_PIXELS];
            "BB" * int(CHRO_PIXELS),

        ]

    @staticmethod
    def size():
        size_sum = 0
        for fmt in AvcRawState.formats():
            size_sum += struct.calcsize(fmt)

        return size_sum

    @staticmethod
    def count(fp):
        return os.fstat(fp.fileno()).st_size // AvcRawState.size()

    def __init__(self, fp):
        fmts = AvcRawState.formats()
        self.header = struct.unpack(fmts[0], fp.read(struct.calcsize(fmts[0])))
        self.pose   = struct.unpack(fmts[1], fp.read(struct.calcsize(fmts[1])))
        self.luma   = struct.unpack(fmts[2], fp.read(struct.calcsize(fmts[2])))
        self.chroma = struct.unpack(fmts[3], fp.read(struct.calcsize(fmts[3])))

    # @property
    def rgb(self, as_byte=True):
        _rgb = []

        for yi in range(FRAME_H):
            row = []
            for xi in range(FRAME_W):
                i = yi * FRAME_W + xi
                j = yi * (FRAME_W >> 1) + (xi >> 1)
                uv = self.chroma[j << 1: (j << 1) + 2]

                row.append([
                    clamp(self.luma[i] + 1.14  * (uv[0] - 128), 0, 255),
                    clamp(self.luma[i] - 0.395 * (uv[1] - 128) - (0.581 * (uv[0] - 128)), 0, 255),
                    clamp(self.luma[i] + 2.033 * (uv[1] - 128), 0, 255),
                    ])
            _rgb.append(row)

        if as_byte:
            return np.array(_rgb)
        else:
            return np.array(_rgb) / 255.0

    # @rgb.setter
    # def rgb(self, rgb):
    #     pass

    

def avc_state_frame(fp, n):
    return [AvcRawState(fp).rgb for _ in range(n)]


# %%
def autoencoder(input_shape=[None, 784],
                n_filters=[3, 10, 10, 10],
                filter_sizes=[3, 3, 3, 3],
                corruption=False):
    """Build a deep denoising autoencoder w/ tied weights.

    Parameters
    ----------
    input_shape : list, optional
        Description
    n_filters : list, optional
        Description
    filter_sizes : list, optional
        Description

    Returns
    -------
    x : Tensor
        Input placeholder to the network
    z : Tensor
        Inner-most latent representation
    y : Tensor
        Output reconstruction of the input
    cost : Tensor
        Overall cost to use for training

    Raises
    ------
    ValueError
        Description
    """
    # %%
    # input to the network
    x = tf.placeholder(
        tf.float32, input_shape, name='x')


    # %%
    # ensure 2-d is converted to square tensor.
    if len(x.get_shape()) == 2:
        x_dim = np.sqrt(x.get_shape().as_list()[1])
        if x_dim != int(x_dim):
            raise ValueError('Unsupported input dimensions')
        x_dim = int(x_dim)
        x_tensor = tf.reshape(
            x, [-1, x_dim, x_dim, n_filters[0]])
    elif len(x.get_shape()) == 4:
        x_tensor = x
    else:
        raise ValueError('Unsupported input dimensions')
    current_input = x_tensor

    # %%
    # Optionally apply denoising autoencoder
    # if corruption:
    #     current_input = corrupt(current_input)

    # %%
    # Build the encoder
    encoder = []
    shapes = []
    for layer_i, n_output in enumerate(n_filters[1:]):
        n_input = current_input.get_shape().as_list()[3]
        shapes.append(current_input.get_shape().as_list())
        W = tf.Variable(
            tf.random_uniform([
                filter_sizes[layer_i],
                filter_sizes[layer_i],
                n_input, n_output],
                -1.0 / math.sqrt(n_input),
                1.0 / math.sqrt(n_input)))
        b = tf.Variable(tf.zeros([n_output]))
        encoder.append(W)
        output = tf.nn.relu(
            tf.add(tf.nn.conv2d(
                current_input, W, strides=[1, 2, 2, 1], padding='SAME'), b))
        current_input = output

    # %%
    # store the latent representation
    z = current_input

    print("Latent vector: " + str(z.get_shape()))

    encoder.reverse()
    shapes.reverse()

    # %%
    # Build the decoder using the same weights
    for layer_i, shape in enumerate(shapes):
        W = encoder[layer_i]
        b = tf.Variable(tf.zeros([W.get_shape().as_list()[2]]))
        output = tf.nn.relu(tf.add(
            tf.nn.conv2d_transpose(
                current_input, W,
                tf.stack([tf.shape(x)[0], shape[1], shape[2], shape[3]]),
                strides=[1, 2, 2, 1], padding='SAME'), b))
        current_input = output

    # %%
    # now have the reconstruction through the network
    y = current_input
    # cost function measures pixel-wise difference
    cost = tf.reduce_sum(tf.square(y - x_tensor))

    # %%
    return {'x': x, 'z': z, 'y': y, 'cost': cost}

def next_batch(fp, n=10):
    batch = []

    for _ in range(n):
        batch.append(AvcRawState(fp).rgb(as_byte=False))

    return np.array(batch)

# %%
def test_mnist():
    """Test the convolutional autoencder using MNIST."""
    # %%
    import tensorflow as tf
    import tensorflow.examples.tutorials.mnist.input_data as input_data
    import matplotlib.pyplot as plt

    # %%
    # load MNIST as before
    # mnist = input_data.read_data_sets('MNIST_data', one_hot=True)
    # mean_img = np.mean(mnist.train.images, axis=0)
    ae = autoencoder(input_shape=[None, FRAME_H, FRAME_W, 3],
                     n_filters=[3, 32, 32, 32],
                     filter_sizes=[3, 3, 3, 3])

    # %%
    learning_rate = 0.01
    optimizer = tf.train.AdamOptimizer(learning_rate).minimize(ae['cost'])

    # %%
    # We create a session to use the graph
    sess = tf.Session()
    sess.run(tf.global_variables_initializer())

    # %%
    # Fit all training data
    batch_size = 10
    n_epochs = 1
    for epoch_i in range(n_epochs):
        fp = open('/home/kirk/avc_data/route.avc.1', 'rb')
        print(str(AvcRawState.count(fp)) + " frames")
        for batch_i in range(AvcRawState.count(fp) // batch_size):
            batch_xs = next_batch(fp, batch_size)
            # train = np.array([img - mean_img for img in batch_xs])
            train = batch_xs.reshape([-1, FRAME_H, FRAME_W, 3])
            sess.run(optimizer, feed_dict={ae['x']: train})
            os.write(1, bytes('.', 'utf-8'))
        print(epoch_i, sess.run(ae['cost'], feed_dict={ae['x']: train}))

    # %%
    # Plot example reconstructions
    n_examples = 10
    fp.close()
    fp = open('/home/kirk/avc_data/route.avc.1', 'rb')
    test_xs = next_batch(fp, n_examples)
    # test_xs_norm = np.array([img - mean_img for img in test_xs])
    test_xs_norm = batch_xs.reshape([-1, FRAME_H, FRAME_W, 3])
    recon = sess.run(ae['y'], feed_dict={ae['x']: test_xs_norm})
    print(recon.shape)
    fig, axs = plt.subplots(2, n_examples, figsize=(10, 2))
    for example_i in range(n_examples):
        axs[0][example_i].imshow(
            np.reshape(test_xs[example_i, :], (FRAME_H, FRAME_W, 3)))
        axs[1][example_i].imshow(
            np.reshape(
                np.reshape(recon[example_i, ...], (FRAME_W * FRAME_H * 3,)),
                (FRAME_H, FRAME_W, 3)))
    fig.show()
    plt.draw()
    plt.waitforbuttonpress()


# %%
if __name__ == '__main__':
    # from PIL import Image
    # fp = open('/home/kirk/avc_data/route.avc.1', 'rb')
    # x = AvcRawState(fp).rgb(as_byte=False)
    #
    # import matplotlib.pyplot as plt
    # fig, axs = plt.subplots(1, 1, figsize=(10, 2))
    # axs.imshow(np.reshape(x, (FRAME_H, FRAME_W, 3)))
    # fig.show()
    # plt.draw()
    # plt.waitforbuttonpress()

    test_mnist()
