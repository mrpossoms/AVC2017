CC=gcc
COLLECTOR_SRC=src/collector.c src/i2c.c src/cam.c

collector: $(COLLECTOR_SRC)
	$(CC) -g $^ -o collector

clean:
	@rm collector
