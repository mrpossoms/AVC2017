CC=gcc
COLLECTOR_SRC=src/collector.c src/i2c.c

collector: $(COLLECTOR_SRC)
	$(CC) $^ -o collector
