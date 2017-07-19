$(eval OS := $(shell uname))

CC=gcc
COLLECTOR_SRC=src/collector.c src/i2c.c
VIEWER_SRC=src/viewer.c
VIEWER_LINK=-lpthread -lm -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo

ifeq ($(OS),Darwin)
	LINK += -lopencv_videoio
endif

collector: $(COLLECTOR_SRC)
	$(CC) $^ -o collector

viewer: $(VIEWER_SRC)
	$(CC) -g -L/usr/local/lib $^ -o viewer $(VIEWER_LINK)
