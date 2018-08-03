import tensorflow as tf
from helpers import PATCH_SIZE


def setup_model(x):
    global PATCH_SIZE
    p = {
        'fc0_w': tf.Variable(tf.truncated_normal([PATCH_SIZE, 128], stddev=0.1)),
        'fc0_b': tf.Variable(tf.constant(0.1, shape=[128])),
        'fc1_w': tf.Variable(tf.truncated_normal([128, 3], stddev=0.1)),
        'fc1_b': tf.Variable(tf.constant(0.1, shape=[3])),
    }

    z0 = tf.matmul(x, p['fc0_w']) + p['fc0_b']
    a0 = tf.nn.relu(z0)

    z1 = tf.matmul(a0, p['fc1_w']) + p['fc1_b']
    h = tf.nn.softmax(z1)

    return p, h