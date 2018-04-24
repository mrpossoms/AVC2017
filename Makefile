$(eval OS := $(shell uname))

CC=gcc
CFLAGS=-g --std=c99 -D_XOPEN_SOURCE=500 
COLLECTOR_SRC=deadreckon.c sys.c BNO055_driver/bno055.c BNO055_driver/bno055_support.c collector.c i2c.c drv_pwm.c cam.c curves.c
PREDICTOR_SRC=predictor.c sys.c i2c.c drv_pwm.c BNO055_driver/bno055.c BNO055_driver/bno055_support.c
INC=-I./src -I./src/BNO055_driver -I./src/linmath
LINK=-lm -lpthread
VIEWER_SRC=viewer.c
VIEWER_LINK=
MASSEUSE_SRC=src/curves.c
MASSEUSE_MAIN=src/masseuse.c
BOTD_SRC=sys.c i2c.c drv_pwm.c BNO055_driver/bno055.c BNO055_driver/bno055_support.c botd.c
BAD_SRC=sys.c goodbad.c
TST_SRC=masseuse_falloff masseuse_bucket

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

obj/%.o: src/%.c magic obj
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) -c $< -o $@

all: viewer collector masseuse

magic: src/structs.h bin
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
	@rm collector viewer masseuse
