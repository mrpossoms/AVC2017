import os
import tensorflow as tf
import signal
from helpers import *
#from fc_2 import setup_model
from cnn_2_16 import setup_model
# from cnn_2_8 import setup_model
# from cnn_3 import setup_model

IS_TRAINING = True
MODEL_STORAGE_PATH = '/etc/bot/predictor/model/'

os.makedirs(MODEL_STORAGE_PATH, exist_ok=True)

def handle_sig_done(*args):
    global IS_TRAINING

    if not IS_TRAINING:
        print("Cancelled")
        exit(0)

    IS_TRAINING = False
    print("Ending training")

signal.signal(signal.SIGINT, handle_sig_done)


def main():
    # Get the set of all the labels and file paths, pre shuffled
    full_set = filenames_labels()

    x = tf.placeholder(tf.float32, [None, PATCH_SIZE])
    y = tf.placeholder(tf.float32, [None, 3])

    p, h = setup_model(x)

    cross_entropy = tf.nn.softmax_cross_entropy_with_logits_v2(labels=y, logits=h)
    loss = tf.reduce_mean(cross_entropy)# + tf.nn.l2_loss(p['fc1_w']) + tf.nn.l2_loss(p['fc0_w'])
    train_step = tf.train.AdamOptimizer().minimize(loss)

    correct_prediction = tf.equal(tf.argmax(h, 1), tf.argmax(y, 1))
    accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

    sess = tf.Session()
    sess.run(tf.global_variables_initializer())

    import random
    last_accuracy = 0
    epochs = 3000
    for e in range(0, epochs):
        sub_ts_x, sub_ts_y = minibatch(full_set, random.randint(0, len(full_set) // 100), size=50)

        sess.run(train_step, feed_dict={x: sub_ts_x, y: sub_ts_y})
        if e % 100 == 0:
            train_accuracy = sess.run(accuracy, feed_dict={x: sub_ts_x, y: sub_ts_y})
            print_stats(
                {
                    'accuracy': last_accuracy
                }, {
                    'accuracy': train_accuracy,
                    'epoch': e,
                    'epoch_total': epochs
                })
            last_accuracy = train_accuracy

        if not IS_TRAINING:
            break

    # Save the learned parameters
    for key in p:
        file_name = key.replace('_', '.')

        with open(MODEL_STORAGE_PATH + file_name, mode='wb') as fp:
            serialize_matrix(sess.run(p[key]), fp)


if __name__ == '__main__':
    main()



