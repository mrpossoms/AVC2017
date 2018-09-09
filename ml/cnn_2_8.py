import tensorflow as tf
from helpers import PATCH_SIZE
from helpers import PATCH_SIDE 


def setup_model(x):
    p = {
        'c0_w': tf.Variable(tf.truncated_normal([3, 3, 3, 16], stddev=0.01)),
        'c0_b': tf.Variable(tf.constant(0.1, shape=[16])),
        'c1_w': tf.Variable(tf.truncated_normal([5, 5, 16, 3], stddev=0.01)),
        'c1_b': tf.Variable(tf.constant(0.1, shape=[3]))
    }

    x = tf.reshape(x, shape=[-1, PATCH_SIDE, PATCH_SIDE, 3])

    z0 = tf.nn.conv2d(x, p['c0_w'], [1, 1, 1, 1], 'VALID') + p['c0_b']
    a0 = tf.nn.relu(z0)
    a = tf.nn.max_pool(a0, ksize=[1, 2, 2, 1], strides=[1, 2, 2, 1], padding='VALID')
    # 7x7

    z1 = tf.nn.conv2d(a, p['c1_w'], [1, 1, 1, 1], 'VALID') + p['c1_b']
    z1 = tf.reshape(z1, [-1, 3])
    h = z1

    return p, h
