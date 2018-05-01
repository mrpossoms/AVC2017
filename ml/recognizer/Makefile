$(eval OS := $(shell uname))

CC=gcc
CFLAGS=-g --std=c99 -D_XOPEN_SOURCE=500
INC=-I./src -I./src/BNO055_driver -I./src/linmath
LINK=-lm -lpthread
TST_SRC=nn_mat_mul nn_mat_func nn_conv_patch nn_mat_load conv_model_test

bin/tests:
	mkdir -p bin/tests

obj:
	mkdir obj

obj/%.o: src/%.c obj
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

tests: bin/tests
	@echo "Building tests..."
	@for source in $(TST_SRC); do\
		($(CC) $(INC) $(CFLAGS) $(MASSEUSE_SRC) src/tests/$$source.c  -o bin/tests/$${source%.*}.bin $(LINK)) || (exit 1);\
	done

test: tests
	@./test_runner.py

clean:
	@rm -rf obj
	@rm collector viewer masseuse
