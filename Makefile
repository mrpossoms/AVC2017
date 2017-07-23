$(eval OS := $(shell uname))

CC=gcc
COLLECTOR_SRC=src/collector.c src/i2c.c src/cam.c
VIEWER_SRC=src/viewer.c
VIEWER_LINK=


ifeq ($(OS),Darwin)
	VIEWER_LINK+=-lpthread -lm -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
	LINK += -lopencv_videoio
endif

viewer: $(VIEWER_SRC)
	$(CC) -g -L/usr/local/lib $^ -o viewer $(VIEWER_LINK)

collector: $(COLLECTOR_SRC)
	$(CC) -g $^ -o collector

clean:
	@rm collector
