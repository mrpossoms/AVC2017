$(eval OS := $(shell uname))

CC=gcc
CFLAGS=-g --std=c99 -D_XOPEN_SOURCE=500
COLLECTOR_SRC=src/collector.c src/i2c.c src/cam.c src/BNO055_driver/*.c
COLLECTOR_INC=-I./src -I./src/BNO055_driver
VIEWER_SRC=src/viewer.c
VIEWER_LINK=
MASSEUSE_SRC=src/curves.c
MASSEUSE_MAIN=src/masseuse.c
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

viewer: $(VIEWER_SRC)
	make magic
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) -L/usr/local/lib $^ -o viewer $(VIEWER_LINK)

collector: $(COLLECTOR_SRC)
	make magic
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(COLLECTOR_INC) $^ -o collector

masseuse: $(MASSEUSE_SRC) $(MASSEUSE_MAIN)
	make magic
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $^  -o masseuse


tests: bin/tests magic
	@echo "Building tests..."
	@for source in $(TST_SRC); do\
		($(CC) -I./src  $(CFLAGS) $(MASSEUSE_SRC) src/tests/$$source.c  -o bin/tests/$${source%.*}.bin $(LINK)) || (exit 1);\
	done

test: tests
	@./test_runner.py

clean:
	@rm collector viewer masseuse
