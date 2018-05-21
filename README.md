# AVC

![simulator running](https://raw.githubusercontent.com/mrpossoms/AVC2017/master/example.gif)

Build: [![CircleCI](https://circleci.com/gh/mrpossoms/AVC2017/tree/master.svg?style=svg)](https://circleci.com/gh/mrpossoms/AVC2017/tree/master)

Autonomous driving software specifically designed for Sparkfun's annual AVC competitions. The system is broken into several different programs each only performing a specific task (in the spirit of unix). The programs may be chained together in a pipeline to easily change the system's function on the fly. For example, the following command would display something like video above.

```bash
$ ./sim | ./predictor -d=n -f | ./actuator -f | ./viewer
```

Each program with the exception of a few can receive some information over stdin, perform some processing or action, the write different information over stdout. The programs in the suite included so far are the following.

### sim
A graphical simulator to replicate the competition environment for testing and experimentation. It is built on OpenGL 4.2 so a modern video card is necessary for its use. sim generates the same type of data.

### collector
Gathers data from physical sensors and forwards it over stdout.

### predictor
Processes data from either collector or sim, and generates an action vector which it emitted over stdout.

### actuator
Directly responsible for the interacting with the hardware of the platform, or the simulator. It receives data over stdin and emits nothing unless specified otherwise.

### viewer
Useful for examining data emitted from other programs. If used, this program should always be the last in a pipeline.

## Requirements

#### Native requirements
* Linux target
* gcc
* GNU Make 3.81
* GNU bash, version 3.2.57
* glfw 3.2.1+

#### Python requirements
* Python 3.5.2
* tensorflow (1.7.0)
* Flask (0.12.2)
* Pillow (4.2.1)
* beautifulsoup4 (4.6.0)
