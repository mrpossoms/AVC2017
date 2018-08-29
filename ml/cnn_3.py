import tensorflow as tf
from helpers import PATCH_SIZE


def setup_model(x):
    p = {
        'c0_w': tf.Variable(tf.truncated_normal([5, 5, 3, 32], stddev=0.01)),
        'c0_b': tf.Variable(tf.constant(0.1, shape=[32])),
        'c1_w': tf.Variable(tf.truncated_normal([5, 5, 32, 32], stddev=0.01)),
        'c1_b': tf.Variable(tf.constant(0.1, shape=[32])),
        'c2_w': tf.Variable(tf.truncated_normal([2, 2, 32, 3], stddev=0.01)),
        'c2_b': tf.Variable(tf.constant(0.1, shape=[3]))
    }

    x = tf.reshape(x, shape=[-1, 16, 16, 3])

    z0 = tf.nn.conv2d(x, p['c0_w'], [1, 1, 1, 1], 'VALID') + p['c0_b']
    a0 = tf.nn.relu(z0)
    a = tf.nn.max_pool(a0, ksize=[1, 2, 2, 1], strides=[1, 2, 2, 1], padding='VALID')
    # 6x6

    z1 = tf.nn.conv2d(a, p['c1_w'], [1, 1, 1, 1], 'VALID') + p['c1_b']
    a1 = tf.nn.relu(z1)
    # a1 = tf.nn.max_pool(a1, ksize=[1, 2, 2, 1], strides=[1, 2, 2, 1], padding='VALID')

    z2 = tf.nn.conv2d(a1, p['c2_w'], [1, 1, 1, 1], 'VALID') + p['c2_b']
    z2 = tf.reshape(z2, [-1, 3])
    h = z2

    return p, h