$(eval OS := $(shell uname))

CC=gcc
CFLAGS=-g --std=c99
COLLECTOR_SRC=src/collector.c src/i2c.c src/cam.c
VIEWER_SRC=src/viewer.c
VIEWER_LINK=
MASSEUSE_SRC=src/masseuse.c


ifeq ($(OS),Darwin)
	VIEWER_LINK+=-lpthread -lm -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
	LINK += -lopencv_videoio
endif

all: viewer collector masseuse

viewer: $(VIEWER_SRC)
	$(CC) $(CFLAGS) -L/usr/local/lib $^ -o viewer $(VIEWER_LINK)

collector: $(COLLECTOR_SRC)
	$(CC) $(CFLAGS) $^ -o collector

masseuse: $(MASSEUSE_SRC)
	$(CC) $(CFLAGS) $^ -o masseuse

clean:
	@rm collector viewer masseuse
