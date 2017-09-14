$(eval OS := $(shell uname))

CC=gcc
CFLAGS=-g --std=c99 -D_XOPEN_SOURCE=500 -Wno-pointer-compare
COLLECTOR_SRC=src/sys.c src/BNO055_driver/*.c src/collector.c src/i2c.c src/drv_pwm.c src/cam.c 
INC=-I./src -I./src/BNO055_driver -I./src/linmath
VIEWER_SRC=src/viewer.c
VIEWER_LINK=
MASSEUSE_SRC=src/curves.c
MASSEUSE_MAIN=src/masseuse.c
BOTD_SRC=src/i2c.c src/drv_pwm.c src/BNO055_driver/*.c src/botd.c
TST_SRC=masseuse_falloff masseuse_bucket

ifeq ($(OS),Darwin)
	VIEWER_LINK+=-lpthread -lm -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
	LINK += -lopencv_videoio
else
	VIEWER_LINK +=-lglfw3 -lGL -lX11 -lXi -lXrandr -lXxf86vm -lXinerama -lXcursor -lrt -lm -pthread -ldl
	CFLAGS += -D_XOPEN_SOURCE=500
endif

bin/tests:
	mkdir -p bin/tests

all: viewer collector masseuse

magic: src/structs.h
	cksum src/structs.h | awk '{split($$0,a," "); print a[1]}' > magic

viewer: magic $(VIEWER_SRC)
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) -L/usr/local/lib $(VIEWER_SRC) -o viewer $(VIEWER_LINK)

collector: magic $(COLLECTOR_SRC)
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $(COLLECTOR_SRC) -o collector -lpthread

masseuse: magic $(MASSEUSE_SRC) $(MASSEUSE_MAIN)
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(MASSEUSE_SRC) $(MASSEUSE_MAIN)  -o masseuse

botd: magic $(BOTD_SRC)
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $(BOTD_SRC) -o botd	

install-bot: collector
	ln -s $(shell pwd)/$^ /usr/bin/$^

install-tools: masseuse viewer
	$(foreach prog, $^, ln -s $(shell pwd)/$(prog) /usr/bin/$(prog);)


tests: bin/tests magic
	@echo "Building tests..."
	@for source in $(TST_SRC); do\
		($(CC) -I./src  $(CFLAGS) $(MASSEUSE_SRC) src/tests/$$source.c  -o bin/tests/$${source%.*}.bin $(LINK)) || (exit 1);\
	done

test: tests
	@./test_runner.py

clean:
	@rm collector viewer masseuse
