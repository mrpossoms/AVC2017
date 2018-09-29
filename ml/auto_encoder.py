"""Tutorial on how to create a convolutional autoencoder w/ Tensorflow.

Parag K. Mital, Jan 2016
"""
import tensorflow as tf
import numpy as np
import math
import struct
import os
import io
import signal
import sys
# from libs.utils import corrupt

FRAME_W = 160
FRAME_H = 120
VIEW_PIXELS = int(FRAME_W * FRAME_H)
LUMA_PIXELS = int(FRAME_W * FRAME_H)
CHRO_PIXELS = int(FRAME_W / 2 * FRAME_H)


def clamp(n, min_, max_):
    return int(max(min_, min(max_, n)))


class AvcRawState():
    formats = [
        # typedef struct {
        #     uint64_t magic;
        #     payload_type_t type;
        # } dataset_hdr_t;
        "QQ",
        # int16_t  rot_rate[3];
        # int16_t  acc[3];
        # float    vel;
        # float    distance;
        # vec3     heading;
        # vec3     position;
        "hhhhhhffffffff",
        # uint8_t luma[LUMA_PIXELS];
        "B" * LUMA_PIXELS,
        # chroma_t chroma[CHRO_PIXELS];
        "BB" * CHRO_PIXELS,
    ]

    @staticmethod
    def size():
        size_sum = 0
        for fmt in AvcRawState.formats:
            size_sum += struct.calcsize(fmt)

        return size_sum

    @staticmethod
    def count(fp):
        return os.fstat(fp.fileno()).st_size // AvcRawState.size()

    def __init__(self, fp):
        fmts = AvcRawState.formats
        self.header = list(struct.unpack(fmts[0], fp.read(struct.calcsize(fmts[0]))))
        self.pose   = list(struct.unpack(fmts[1], fp.read(struct.calcsize(fmts[1]))))
        self.luma   = np.frombuffer(fp.read(LUMA_PIXELS), dtype=np.uint8)
        self.chroma = np.frombuffer(fp.read(CHRO_PIXELS << 1), dtype=np.uint8)

    def write(self, fp):
        fmts = AvcRawState.formats
        fp.write(struct.pack(fmts[0], *tuple(self.header)))
        fp.write(struct.pack(fmts[1], *tuple(self.pose)))
        fp.write(self.luma.tobytes())
        fp.write(self.chroma.tobytes())

    @property
    def rgb(self):
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

        return np.array(_rgb)

    @rgb.setter
    def rgb(self, rgb):
        for yi in range(FRAME_H):
            for xi in range(FRAME_W):
                i = yi * FRAME_W + xi
                j = yi * (FRAME_W >> 1) + (xi >> 1)
                r, g, b = rgb[yi][xi]

                self.luma[i] = clamp((r + g + b) // 3, 0, 255)

                # r = luma[i] + 1.14 * uv[0]
                # (uv[0] - 128) = r - luma[i] / 1.14
                self.chroma[(j << 1) + 0] = clamp(128 + int((r - self.luma[i]) / 1.14), 0, 255)
                self.chroma[(j << 1) + 1] = clamp(128 + int((b - self.luma[i]) / 2.033), 0, 255)

                # b = luma[i] + 2.033 * (uv[1])
                # (b - luma[i]) / 2.033 = uv[1]

                # r = l + A
                # g = l + B
                # b = l + C

class PoseDB:
    def __init__(self, capacity=1000):
        self._cap = capacity
        self._encodings = []
        self._poses = []
        self._means = None
        self._dimensionality = None
        pass

    def nearest(self, encoding):
        nearest_dist, nearest_enc, nearest_i = np.inf, None, None

        for i, enc in enumerate(self._encodings):
            delta = encoding - enc
            d = np.sum(delta ** 2)
            if d < nearest_dist:
                nearest_dist = d
                nearest_enc = enc

        return nearest_enc, nearest_dist, nearest_i

    def append(self, encoding, pose, prox_merge=1, prox_replace=4, replace_prob=0.5):
        if self._dimensionality is None:
            self._dimensionality = encoding.size

        if len(self._encodings) > self._cap:
            enc, dist, i = self.nearest(encoding)

            # if self._means is None:
            #
            #     self._means = [ for _ in range(self._cap)]

            if prox_merge > dist < prox_replace:
                # sys.stderr.write('[merge] dist: {}\n'.format(dist))
                # sys.stderr.flush()
                enc += encoding
                enc /= 2

                return enc
            elif dist >= prox_replace:
                if np.random.rand() < replace_prob:
                    j = np.random.randint(0, self._cap)
                    self._encodings[j] = enc
                    self._poses[j] = pose
                    # sys.stderr.write('[replace] dist: {}\n'.format(dist))
                    # sys.stderr.flush()
            else:
                # sys.stderr.write('[retrieved] dist: {}\n'.format(dist))
                # sys.stderr.flush()
                pass
        else:
            self._encodings.append(encoding)
            self._poses.append(pose)

        return None


def avc_state_frame(fp, n):
    return [AvcRawState(fp).rgb for _ in range(n)]


# %%
def autoencoder(input_shape=[None, 784],
                n_filters=[3, 10, 10, 10],
                filter_sizes=[3, 3, 3, 3],
                strides=[2, 2, 2, 2],
                # pool=[False, False, False, False],
                corruption=False,
                parameters={}):
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
    import helpers

    # %%
    PADDING = 'SAME'
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
        n_stride = strides[layer_i]
        shapes.append(current_input.get_shape().as_list())
        k_sz = filter_sizes[layer_i]
        den = math.sqrt(n_input)

        W = tf.Variable(tf.random_uniform([k_sz, k_sz, n_input, n_output], -1.0 / den, 1.0 / den))
        b = tf.Variable(tf.zeros([n_output]))

        w_name = 'w_cnn_enc_' + str(layer_i)
        b_name = 'b_cnn_enc_' + str(layer_i)

        for name in [w_name, b_name]:
            if name in parameters:
                with open('ae_model/' + name, 'rb') as fp:
                    if name is w_name:
                        W = tf.Variable(helpers.deserialize_matrix(fp))
                    elif name is b_name:
                        b = tf.Variable(helpers.deserialize_matrix(fp))

        parameters[w_name] = W
        parameters[b_name] = b
        encoder.append(W)
        output = tf.nn.relu(
            tf.add(tf.nn.conv2d(
                current_input, W, strides=[1, n_stride, n_stride, 1], padding=PADDING), b))

        current_input = output

    # %%
    # store the latent representation
    z = current_input

    print("Latent vector: " + str(z.get_shape()))

    encoder.reverse()
    shapes.reverse()
    # pool.reverse()
    strides.reverse()
    filter_sizes.reverse()

    # %%
    # Build the decoder using the same weights
    for layer_i, shape in enumerate(shapes):
        W = encoder[layer_i]
        b = tf.Variable(tf.zeros([W.get_shape().as_list()[2]]))
        n_input = current_input.get_shape().as_list()[3]
        n_stride = strides[layer_i]

        output = tf.nn.relu(tf.add(
            tf.nn.conv2d_transpose(
                current_input, W,
                tf.stack([tf.shape(x)[0], shape[1], shape[2], shape[3]]),
                strides=[1, n_stride, n_stride, 1], padding=PADDING), b))
        current_input = output
        print(current_input.get_shape().as_list())

    # %%
    # now have the reconstruction through the network
    y = current_input
    # cost function measures pixel-wise difference
    cost = tf.reduce_sum(tf.square(y - x_tensor))

    # %%
    return {'x': x, 'z': z, 'y': y, 'cost': cost, 'parameters': parameters}


def next_batch(fp, n=10, shuffle=True, data='full'):
    batch = []
    examples = AvcRawState.count(fp)
    size = AvcRawState.size()

    for _ in range(n):
        if shuffle:
            fp.seek(np.random.randint(0, examples - 1) * size, io.SEEK_SET)

        if data is 'full':
            batch.append(AvcRawState(fp))
        elif data is 'rgb':
            batch.append(AvcRawState(fp).rgb / 255.0)
        elif data is 'luma':
            batch.append(AvcRawState(fp).luma / 255.0)

    return np.array(batch)


RUNNING = True


def handle_sig_done(*args):
    global RUNNING

    if not RUNNING:
        print("Aborting")
        exit(0)

    RUNNING = False
    print("Terminating")

signal.signal(signal.SIGINT, handle_sig_done)


# %%
def main(args):
    """Test the convolutional autoencder using MNIST."""
    # %%
    import tensorflow as tf

    DEPTH = 1

    # %%
    # load MNIST as before
    # mnist = input_data.read_data_sets('MNIST_data', one_hot=True)
    # mean_img = np.mean(mnist.train.images, axis=0)
    param_dic = {}
    for name in os.listdir('ae_model'): param_dic[name] = True

    ae = autoencoder(input_shape=[None, FRAME_H, FRAME_W, DEPTH],
                     n_filters=[DEPTH, 16, 16, 16, 16, 16],
                     filter_sizes=[3, 3, 3, 3, 3, 3],
                     strides=[2, 2, 2, 2, 2, 2],
                     parameters=param_dic)


                     #n_filters=[DEPTH, 32, 32, 32, 32, 32, 32],
                     #filter_sizes=[3, 3, 3, 3, 3, 3, 3],
                     #strides=[2, 2, 2, 2, 2, 2, 2],
                     #pool=[False, False, False, False, False, False])

                     # n_filters=[DEPTH, 16, 16, 16, 16],
                     # filter_sizes=[5, 3, 3, 3],
                     # strides=[2, 2, 2, 2],
                     # pool=[False, False, False, False, False])

    # %%
    learning_rate = 0.0001
    optimizer = tf.train.AdamOptimizer(learning_rate).minimize(ae['cost'])

    # %%
    # We create a session to use the graph
    sess = tf.Session()
    sess.run(tf.global_variables_initializer())

    if args.train:
        do_training(DEPTH, ae, optimizer, sess)
    else:
        do_mapping(DEPTH, ae, sess)


def do_mapping(DEPTH, ae, sess):
    import matplotlib.pyplot as plt
    hl = plt.plot([], [], markersize=2, marker='.')[0]
    plt.axis([-50, 50, -50, 50])
    plt.ion()
    plt.show()
    ax = hl._axes
    # plt.show()

    db = PoseDB(capacity=300)
    hit_cap = False
    i = 0

    while RUNNING:
        i += 1
        state = AvcRawState(sys.stdin.buffer)
        x = state.luma / 255.0
        x = x.reshape([-1, FRAME_H, FRAME_W, DEPTH])
        enc = sess.run(ae['z'], feed_dict={ae['x']: x})[0]

        merged = db.append(enc, state.pose)

        if len(db._encodings) >= 1000 and not hit_cap:
            sys.stderr.write('PoseDB: Hit cap\n')
            sys.stderr.flush()
            hit_cap = True

        y_ = sess.run(ae['y'], feed_dict={ae['x']: x})[0]
        state.luma = np.clip(y_ * 255, 0, 255).astype(dtype=np.uint8)
        state.chroma = state.luma
        state.write(sys.stdout.buffer)

        # ax.clear

        if i % 10 == 0:
            plt.clf()
            for pose in db._poses:
                pos = pose[-4:-1]
                plt.plot(pos[1], pos[2], markersize=1, marker='.')
                #
                # plt.pause(0.05)
                # sys.stderr.write(str(pos) + '\n')

            plt.pause(0.0001)
            plt.draw()
            # plt.relim()
            # plt.autoscale_view()




def do_training(DEPTH, ae, optimizer, sess):
    import matplotlib.pyplot as plt

    # %%
    # Fit all training data
    batch_size = 100
    n_epochs = 200
    for epoch_i in range(n_epochs):

        if not RUNNING:
            break

        with open('avc_data/route.avc.1', 'rb') as fp:
            print(str(AvcRawState.count(fp)) + " frames")
            for batch_i in range(AvcRawState.count(fp) // batch_size):
                # for batch_i in range(2):
                batch_xs = next_batch(fp, batch_size, shuffle=True, data='luma')
                # train = np.array([img - mean_img for img in batch_xs])
                train = batch_xs.reshape([-1, FRAME_H, FRAME_W, DEPTH])
                sess.run(optimizer, feed_dict={ae['x']: train})
                os.write(1, bytes('.', 'utf-8'))
                # os.write(1, bytes('.'))
            print(epoch_i, sess.run(ae['cost'], feed_dict={ae['x']: train}))

    # %%
    # Plot example reconstructions
    with open('avc_data/route.avc.2', 'rb') as rd_fp:
        with open('avc_data/decoded', 'wb') as wr_fp:
            total = AvcRawState.count(rd_fp)
            for i in range(total):

                state = next_batch(rd_fp, 1, shuffle=False, data='full')[0]
                x = state.luma / 255.0
                x = x.reshape([-1, FRAME_H, FRAME_W, DEPTH])
                y_ = sess.run(ae['y'], feed_dict={ae['x']: x})[0]
                state.luma = np.clip(y_ * 255, 0, 255).astype(dtype=np.uint8)
                state.chroma = state.luma
                state.write(wr_fp)

                if i % 100 == 0:
                    print('{}%'.format(i * 100 // total))

    # save the model parameters
    if input('Store model parameters? [Y/n] ').lower() is 'y':
        # save model parameters
        from helpers import serialize_matrix
        for p_name in ae['parameters']:
            with open('ae_model/' + p_name, 'wb') as fp:
                serialize_matrix(sess.run(ae['parameters'][p_name]), fp)
    n_examples = 10
    fp = open('avc_data/route.avc.1', 'rb')
    test_xs = next_batch(fp, n_examples, data='luma')
    test_xs_norm = test_xs.reshape([-1, FRAME_H, FRAME_W, DEPTH])
    recon = sess.run(ae['y'], feed_dict={ae['x']: test_xs_norm})
    print(recon.shape)
    fig, axs = plt.subplots(2, n_examples, figsize=(10, 2))
    img_shape = (FRAME_H, FRAME_W)
    if DEPTH > 1:
        img_shape = (FRAME_H, FRAME_W, DEPTH)
    for example_i in range(n_examples):
        axs[0][example_i].imshow(
            np.reshape(test_xs[example_i, :], img_shape))
        axs[1][example_i].imshow(
            np.reshape(
                np.reshape(recon[example_i, ...], (FRAME_W * FRAME_H * DEPTH,)),
                img_shape))
    fig.show()
    plt.draw()
    plt.waitforbuttonpress()


# %%
if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--train", help="Set to perform training", action="store_true")
    args = parser.parse_args()
    main(args)
