import tensorflow as tf
import numpy as np
from PIL import Image
from random import shuffle
import sys
import os
import time
import signal
import struct

IS_TRAINING = True

PATCH_SIDE = 16
X_SIZE = (3 * (PATCH_SIDE ** 2))

def handle_sig_done(*args):
    global IS_TRAINING

    if not IS_TRAINING:
        print("Cancelled")
        exit(0)

    IS_TRAINING = False
    print("Ending training")

signal.signal(signal.SIGINT, handle_sig_done)

def process_example(x):
    # mu = np.average(x, axis=(0, 1))
    return (np.array((x).flatten(), dtype=np.float32) / 255.0) - 0.5

def activation_map(model, in_path, out_path):
    img = Image.open(in_path).convert('RGB')
    img_array = np.array(img)
    w, h, d = img_array.shape
    stride = 8

    act_w = ((w - PATCH_SIDE) + 1) // stride
    act_h = ((h - PATCH_SIDE) + 1) // stride

    px_h = act_h * stride
    px_w = act_w * stride

    act_map = np.zeros((w, h, 3))


    def in_fn():
        X = []
        Y = []

        for y in range(0, px_h, stride):
            for x in range(0, px_w, stride):
                patch = img_array[x:x+PATCH_SIDE, y:y+PATCH_SIDE]

                try:
                    flat_patch = process_example(patch).reshape((1, X_SIZE))

                    X.append(flat_patch)
                    Y.append([0,0,0])
                except:
                    pass

        my_input_fn = tf.estimator.inputs.numpy_input_fn(
            x={"x": np.array(X, np.float32).reshape((len(X), X_SIZE))},
            y=np.array(Y, np.float32).reshape((len(Y), 3)),
            batch_size=len(X),
            num_epochs=1,
            shuffle=False)

        return my_input_fn
        # return np.array(X, np.float32).reshape((len(X), X_SIZE)), np.array(Y, np.float32).reshape((len(Y), 3))

    i = 0
    for single_prediction in model.predict(in_fn()):
        x = i % act_w
        y = i // act_w
        i += 1

        _y = single_prediction['probabilities']
        # _y /= _y.max()

        color = np.array([[[1, 1, 1]]], dtype=np.float32)


        color *= np.array(_y)

        ix = x * stride
        iy = y * stride
        act_map[ix:ix+PATCH_SIDE, iy:iy+PATCH_SIDE] = img_array[ix:ix+PATCH_SIDE, iy:iy+PATCH_SIDE] * color


    Image.fromarray(act_map.astype(np.uint8)).save(out_path, "PNG")


def filenames_labels():
    files_labels = []

    for f in os.listdir('imgs/0'):
        files_labels += [('imgs/0/' + f, [1, 0, 0])]

    for f in os.listdir('imgs/1'):
        files_labels += [('imgs/1/' + f, [0, 1, 0])]

    for f in os.listdir('imgs/2'):
        files_labels += [('imgs/2/' + f, [0, 0, 1])]

    shuffle(files_labels)

    return files_labels


def array_from_file(path):
    img = Image.open(path).convert('RGB')
    return np.array(img)[0:PATCH_SIDE,0:PATCH_SIDE,:]


def show_example(X, i, title=None):
    test = Image.fromarray(((X[i]) * 256).reshape((PATCH_SIDE, PATCH_SIDE, 3)).astype(np.uint8))
    test.show(title=title)


def minibatch(files_labels, index, size=100):
    X, Y = [], []

    if size == -1:
        index = 0
        size = len(files_labels)

    for file, label in files_labels[index * size: index * size + size]:
        img_array = array_from_file(file)
        assert(img_array.size == X_SIZE)

        X += [process_example(img_array)]
        Y += [label]

    m = len(X)

    X = np.array(X, dtype=np.float32).reshape((m, X_SIZE))

    return (X, np.array(Y, dtype=np.float32).reshape((m, 3)))


def main(argv):
    # train({
    #     'layer_sizes': [256, 256, 256],
    #     'learning_rate': 0.0001,
    #     'l2_reg_term': 0.001,
    #     'epochs': -1             # -1: only stops at SIGINT
    # })

    tf.logging.set_verbosity(tf.logging.INFO)
    train({'layer_sizes': [512, 512], 'learning_rate': 0.0009, 'l2_reg_term': 0.009, 'epochs': -1, 'score': 0.81375921375921378})

    # hyper_parameter_search({
    #    'layer_sizes': ([256, 512], 1, 3),
    #    'learning_rate': (0.0, 0.0006, 0.001),
    #    'l2_reg_term': (0.0, 0.0001, 0.01)
    # })


def hyper_parameter_search(ranges, candidates=100):

    hyper_param_set = []

    for _ in range(candidates):
        hyper_params = {}

        for param in ranges:
            t, low, high = ranges[param]
            rnd = low + (np.random.random() * (high - low))

            if type(t) is int:
                rnd = int(rnd)
            elif type(t) is float:
                rnd = float(rnd)
            elif type(t) is list:
                layer_size = int(np.random.random() * (t[1] - t[0]) + t[0])
                rnd = [layer_size] * int(np.ceil(rnd))

            hyper_params[param] = rnd

        hyper_params['epochs'] = 10
        hyper_param_set.append(hyper_params)

    i = 0
    best = 0
    for hp in hyper_param_set:
        print('%d/100 evaluated' % i)
        ts_score, ds_score = train(hp)

        score = 2 / ((1 / ts_score) + (1 / ds_score))
        hp['score'] = score
        i += 1

        if hp['score'] > best:
            best = hp['score']
            print(hp)

    def score_key(param):
        return param['score']

    hyper_param_set = sorted(hyper_param_set, key=score_key, reverse=True)
    print(hyper_param_set[0:3])


def model(features, labels, mode):
    input_layer = tf.reshape(features["x"], [-1, PATCH_SIDE, PATCH_SIDE, 3])

    # Convolutional Layer #1
    conv1 = tf.layers.conv2d(
        inputs=input_layer,
        filters=16,
        kernel_size=[3, 3],
        padding="same",
        activation=tf.nn.relu)

    # Pooling Layer #1
    pool1 = tf.layers.max_pooling2d(inputs=conv1, pool_size=[2, 2], strides=2)

    # Convolutional Layer #2 and Pooling Layer #2
    conv2 = tf.layers.conv2d(
        inputs=pool1,
        filters=32,
        kernel_size=[3, 3],
        padding="same",
        activation=tf.nn.relu)
    pool2 = tf.layers.max_pooling2d(inputs=conv2, pool_size=[2, 2], strides=2)

    # Convolutional Layer #2 and Pooling Layer #2
    conv3 = tf.layers.conv2d(
        inputs=pool2,
        filters=64,
        kernel_size=[3, 3],
        padding="same",
        activation=tf.nn.relu)
    pool3 = tf.layers.max_pooling2d(inputs=conv3, pool_size=[2, 2], strides=2)

    # Dense Layer
    pool_flat = tf.reshape(pool3, [-1, 2 * 2 * 64])
    dense = tf.layers.dense(inputs=pool_flat, units=256, activation=tf.nn.relu)
    # dropout = tf.layers.dropout(
    #     inputs=dense, rate=0.4, training=mode == tf.estimator.ModeKeys.TRAIN)

    # Logits Layer
    logits = tf.layers.dense(inputs=dense, units=3)

    predictions = {
        # Generate predictions (for PREDICT and EVAL mode)
        "classes": logits,
        # Add `softmax_tensor` to the graph. It is used for PREDICT and by the
        # `logging_hook`.
        "probabilities": tf.nn.softmax(logits, name="softmax_tensor")
    }

    if mode == tf.estimator.ModeKeys.PREDICT:
        return tf.estimator.EstimatorSpec(mode=mode, predictions=predictions)

    # Calculate Loss (for both TRAIN and EVAL modes)
    loss = tf.losses.softmax_cross_entropy(labels, logits)

    # Configure the Training Op (for TRAIN mode)
    if mode == tf.estimator.ModeKeys.TRAIN:
        optimizer = tf.train.AdamOptimizer(learning_rate=0.001)
        train_op = optimizer.minimize(
            loss=loss,
            global_step=tf.train.get_global_step())
        return tf.estimator.EstimatorSpec(mode=mode, loss=loss, train_op=train_op)

    # Add evaluation metrics (for EVAL mode)
    eval_metric_ops = {
        "accuracy": tf.metrics.accuracy(
            labels=labels, predictions=predictions["classes"])}
    return tf.estimator.EstimatorSpec(
        mode=mode, loss=loss, eval_metric_ops=eval_metric_ops)


def export_model(model):
    for param_name in model.get_variable_names():
        comps = param_name.split('/')
        name = comps[0]

        if len(comps) < 2: continue
        if comps[-1] in ['kernel', 'bias']:
            with open('model/' + param_name.replace('/', '.'), mode='wb') as file:
                param = model.get_variable_value(param_name)
                shape = param.shape

                # We will be converting the weight tensor into
                # a matrix to make it usable in the predictor implementation
                # if len(shape) == 4:
                    # shape = (shape[3], np.prod(shape[0:3]))

                file.write(struct.pack('b', len(shape)))
                for d in shape:
                    file.write(struct.pack('i', d))

                # if len(param.shape) == 4:
                #     for f in range(param.shape[3]):
                #         filter = param[:,:,:,f]
                #
                #         for w in filter.flatten():
                #             file.write(struct.pack('f', w))
                # else:
                for w in param.flatten():
                    file.write(struct.pack('f', w))


def train(hyper_params):
    # Get the set of all the labels and file paths, pre shuffled
    full_set = filenames_labels()

    # find total number of samples, compute count of complete batches
    m = len(full_set)
    n = 1000 # mini batch size
    batches = m // 1000

    # Split in to a training and test set
    ts_batches = int(np.floor(batches * 0.75)) - 1
    training_set = full_set[0:ts_batches * n]
    dev_set = full_set[ts_batches * n:-1]

    texture_classifier = tf.estimator.Estimator(
        model_fn=model, model_dir="/tmp/texture_convnet_model")

    tensors_to_log = {"probabilities": "softmax_tensor"}
    logging_hook = tf.train.LoggingTensorHook(
        tensors={}, every_n_iter=50)

    ts_X, ts_Y = minibatch(training_set, index=0, size=100)
    my_input_fn = tf.estimator.inputs.numpy_input_fn(
        x={"x": ts_X},
        y=ts_Y,
        batch_size=100,
        num_epochs=None,
        shuffle=True)

    texture_classifier.train(my_input_fn, [logging_hook], 100)

    ds_X, ds_Y = minibatch(dev_set, 0, size=100)
    epochs = hyper_params['epochs']
    ts_score, ds_score = 0, 0

    # while IS_TRAINING and epochs != 0:
    #     epochs -= 1
    #     for i in range(ts_batches):
    #         if not IS_TRAINING:
    #             break
    #
    #         X, Y = minibatch(training_set, i, size=n)
    #         model.fit(X, Y)
    #
    #         if i % 40 == 0:
    #             ts_score = model.score(X, Y)
    #             print("TS: %f - DS: %f" % (ts_score, model.score(ds_X, ds_Y)))
    #
    # ds_score = model.score(ds_X, ds_Y)
    # print('Dev set score: %f' % ds_score)

    export_model(texture_classifier)

    if epochs < 0:
        for i in range(4):
            activation_map(texture_classifier, "test%d.png" % i, "act_map%d.png" % i)

    return ts_score, ds_score

if __name__ == '__main__':
    main(sys.argv)
