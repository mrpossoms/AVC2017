import tensorflow as tf
import signal
from helpers import *

IS_TRAINING = True

def handle_sig_done(*args):
    global IS_TRAINING

    if not IS_TRAINING:
        print("Cancelled")
        exit(0)

    IS_TRAINING = False
    print("Ending training")

signal.signal(signal.SIGINT, handle_sig_done)


def setup_model(X, Y):
    global PATCH_SIZE
    p = {
        'fc0_w': tf.Variable(tf.truncated_normal([PATCH_SIZE, 128], stddev=0.1)),
        'fc0_b': tf.Variable(tf.constant(0.1, shape=[128])),
        'fc1_w': tf.Variable(tf.truncated_normal([128, 3], stddev=0.1)),
        'fc1_b': tf.Variable(tf.constant(0.1, shape=[3])),
    }

    z0 = tf.matmul(X, p['fc0_w']) + p['fc0_b']
    a0 = tf.nn.relu(z0)

    z1 = tf.matmul(a0, p['fc1_w']) + p['fc1_b']
    h = tf.nn.softmax(z1)

    return p, h


def main():
    # Get the set of all the labels and file paths, pre shuffled
    full_set = filenames_labels()

    X = tf.placeholder(tf.float32, [None, PATCH_SIZE])
    Y = tf.placeholder(tf.float32, [None, 3])

    p, h = setup_model(X, Y)

    cross_entropy = tf.nn.softmax_cross_entropy_with_logits_v2(labels=Y, logits=h)
    loss = tf.reduce_mean(cross_entropy)# + tf.nn.l2_loss(p['fc1_w']) + tf.nn.l2_loss(p['fc0_w'])
    train_step = tf.train.AdamOptimizer().minimize(loss)

    correct_prediction = tf.equal(tf.argmax(h, 1), tf.argmax(Y, 1))
    accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

    sess = tf.Session()
    sess.run(tf.global_variables_initializer())

    import random
    for e in range(0, 1000):
        sub_ts_X, sub_ts_Y = minibatch(full_set, random.randint(0, len(full_set) // 100), size=50)

        sess.run(train_step, feed_dict={X: sub_ts_X, Y: sub_ts_Y})
        if e % 100 == 0:
            # train_h = sess.run(h, feed_dict={X: sub_ts_X, Y: sub_ts_Y})
            # train_prediction = sess.run(correct_prediction, feed_dict={X: sub_ts_X, Y: sub_ts_Y})
            train_accuracy = sess.run(accuracy, feed_dict={X: sub_ts_X, Y: sub_ts_Y})
            print('step %d, training accuracy %f' % (e, train_accuracy))

        if not IS_TRAINING:
            break

    # Save the learned parameters
    for key in p:
        file_name = key.replace('_', '.')

        with open('/var/model/' + file_name, mode='wb') as fp:
            serialize_matrix(sess.run(p[key]), fp)


if __name__ == '__main__':
    main()



