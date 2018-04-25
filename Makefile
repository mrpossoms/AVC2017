$(eval OS := $(shell uname))

CC=gcc
CXX=g++
CFLAGS=-g --std=c99 -D_XOPEN_SOURCE=500
CXXFLAGS=--std=c++11 -g
COLLECTOR_SRC=deadreckon.c sys.c BNO055_driver/bno055.c BNO055_driver/bno055_support.c collector.c i2c.c drv_pwm.c cam.c curves.c
PREDICTOR_SRC=predictor.c sys.c i2c.c drv_pwm.c BNO055_driver/bno055.c BNO055_driver/bno055_support.c
INC=-I./src -I./src/BNO055_driver -I./src/linmath -I./src/seen/src
LINK=-lm -lpthread
VIEWER_SRC=viewer.c
VIEWER_LINK=
MASSEUSE_SRC=src/curves.c
MASSEUSE_MAIN=src/masseuse.c
BOTD_SRC=sys.c i2c.c drv_pwm.c BNO055_driver/bno055.c BNO055_driver/bno055_support.c botd.c
BAD_SRC=sys.c goodbad.c
TST_SRC=masseuse_falloff masseuse_bucket

SIM_SRC=src/sim.cpp src/seen/demos/src/sky.cpp
SIM_INC=-Isrc/seen/demos/src/

ifeq ($(OS),Darwin)
	VIEWER_LINK +=-lpthread -lm -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
	VIEWER_LINK += -lopencv_videoio
else
	VIEWER_LINK +=-lglfw3 -lGL -lX11 -lXi -lXrandr -lXxf86vm -lXinerama -lXcursor -lrt -lm -pthread -ldl
	CFLAGS += -D_XOPEN_SOURCE=500
endif

bin/tests:
	mkdir -p bin/tests

obj:
	mkdir obj
	mkdir obj/BNO055_driver

bin:
	mkdir bin

bin/data: bin
	ln -sf $(shell pwd)/src/seen/demos/data/ bin/data

obj/%.o: src/%.c magic obj
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) -c $< -o $@

all: viewer collector masseuse

src/linmath.h:
	git clone https://github.com/mrpossoms/linmath.h src/linmath.h
	make -C src/linmath.h install

src/seen:
	git clone https://github.com/mrpossoms/Seen src/seen
	make -C src/seen static

magic: src/structs.h src/linmath.h src/seen bin/data
	cksum src/structs.h | awk '{split($$0,a," "); print a[1]}' > magic

bin/structsize: bin
	$(CC) $(CFLAGS) $(INC) src/size.c -o structsize

bin/viewer: $(addprefix src/,$(VIEWER_SRC)) magic
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) -L/usr/local/lib $(INC) $< -o viewer $(VIEWER_LINK) $(LINK)

bin/collector: $(addprefix obj/,$(COLLECTOR_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/predictor: $(addprefix obj/,$(PREDICTOR_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/bad: $(addprefix obj/,$(BAD_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/good: $(addprefix obj/,$(BAD_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)


bin/botd: $(addprefix obj/,$(BOTD_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/masseuse: magic $(MASSEUSE_SRC) $(MASSEUSE_MAIN)
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $(MASSEUSE_SRC) $(MASSEUSE_MAIN)  -o masseuse

bin/sim: magic
	$(CXX) $(CXXFLAGS) -DMAGIC=$(shell cat magic) $(INC) $(SIM_INC) $(SIM_SRC) -o $@ $(VIEWER_LINK) $(LINK) -lpng src/seen/lib/libseen.a

/var/predictor/color/bad:
	mkdir -p $@
	chmod -R 777 $@

/var/predictor/color/good:
	mkdir -p $@
	chmod -R 777 $@

install-bot: bin/predictor bin/collector bin/structsize bin/bad bin/good /var/predictor/color/bad /var/predictor/color/good
	$(foreach prog, $^, ln -s $(shell pwd)/$(prog) /usr/bin/$(prog);)

install-tools: bin/masseuse bin/viewer bin/structsize
	$(foreach prog, $^, ln -s $(shell pwd)/$(prog) /usr/bin/$(prog);)


tests: bin/tests magic
	@echo "Building tests..."
	@for source in $(TST_SRC); do\
		($(CC) $(INC)  $(CFLAGS) $(MASSEUSE_SRC) src/tests/$$source.c  -o bin/tests/$${source%.*}.bin $(LINK)) || (exit 1);\
	done

test: tests
	@./test_runner.py

clean:
	@rm -rf obj
	@rm -rf bin
