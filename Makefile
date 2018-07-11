$(eval OS := $(shell uname))

CC=gcc
CXX=g++
CFLAGS=-g --std=c99 -D_XOPEN_SOURCE=500 -Wall -Wno-implicit-function-declaration
CXXFLAGS=--std=c++11 -g
INC=-Isrc -Isrc/libnn/src -Isrc/drivers -Isrc/drivers/src/BNO055_driver -Isrc/linmath.h -Isrc/seen/src -Isrc/json -Iml/recognizer/src
LINK=-lm -lpthread

DRIVER_SRC= drivers/BNO055_driver/bno055.c drivers/BNO055_driver/bno055_support.c drivers/drv_pwm.c i2c.c
BASE_SRC = sys.c $(DRIVER_SRC)

COLLECTOR_SRC=deadreckon.c collector.c cam.c $(BASE_SRC)
PREDICTOR_FLAGS=-funsafe-math-optimizations -march=native -O3 -ftree-vectorize
PREDICTOR_SRC=predictor.c $(BASE_SRC)
PREDICTOR_LINK=src/libnn/lib/libnn.a
ACTUATOR_SRC=actuator.c $(BASE_SRC)

VIEWER_SRC=sys.c viewer.c
VIEWER_LINK=
BOTD_SRC=sys.c botd.c $(DRIVER_SRC)
TST_SRC=masseuse_falloff masseuse_bucket

SIM_SRC=src/sim/sim.cpp src/sys.c src/seen/demos/src/sky.cpp
SIM_INC=-Isrc/seen/demos/src/

TRAINX_SRC= trainx.c $(BASE_SRC)

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
	mkdir -p obj/drivers/BNO055_driver

bin:
	mkdir bin
	cp actions.cal bin/

external:


bin/data: bin
	ln -sf $(shell pwd)/src/seen/demos/data/ bin/data

bin/scene.json: bin
	cp .scene.json bin/scene.json

obj/%.o: src/%.c magic obj
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) -c $< -o $@

all: viewer collector masseuse

src/libnn:
	git clone https://github.com/mrpossoms/libnn src/libnn

src/libnn/lib/libnn.a: src/libnn
	make -C src/libnn static

src/json:
	mkdir -p src/json
	wget https://raw.githubusercontent.com/nlohmann/json/master/single_include/nlohmann/json.hpp --output-document src/json/json.hpp

src/linmath.h:
	git clone https://github.com/mrpossoms/linmath.h src/linmath.h
	make -C src/linmath.h install

src/seen:
	git clone https://github.com/mrpossoms/Seen src/seen
	make -C src/seen static

magic: src/structs.h src/linmath.h src/libnn/lib/libnn.a src/seen src/json bin/data bin/scene.json
	cksum src/structs.h | awk '{split($$0,a," "); print a[1]}' > magic

bin/structsize: bin
	$(CC) $(CFLAGS) $(INC) src/size.c -o structsize

bin/collector: $(addprefix obj/,$(COLLECTOR_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/predictor: $(addprefix obj/,$(PREDICTOR_SRC:.c=.o))
	$(CC) $(CFLAGS) $(PREDICTOR_FLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK) $(PREDICTOR_LINK)

bin/actuator: $(addprefix obj/,$(ACTUATOR_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/botd: $(addprefix obj/,$(BOTD_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/viewer: $(addprefix obj/,$(VIEWER_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(LIB_PATHS) $(INC) $(LIB_INC) $^ -o $@ $(VIEWER_LINK) $(LINK)

bin/sim: magic
	$(CXX) $(CXXFLAGS) -DMAGIC=$(shell cat magic) $(LIB_PATHS) $(INC) $(LIB_INC) $(SIM_INC) $(SIM_SRC) -o $@ $(VIEWER_LINK) $(LINK) -lpng src/seen/lib/libseen.a

bin/trainx: $(addprefix obj/,$(TRAINX_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK) -lpng -lz


/var/predictor/color/bad:
	mkdir -p $@
	chmod -R 777 $@

/var/predictor/color/good:
	mkdir -p $@
	chmod -R 777 $@

bot-utils: bin/predictor bin/actuator bin/collector bin/trainx bin/botd
	@echo "Built bot utilities"

install-bot: bot-utils 
	$(foreach prog, $^, ln -s $(shell pwd)/$(prog) /usr/$(prog);)

install-tools: bin/viewer bin/sim
	$(foreach prog, $^, ln -s $(shell pwd)/$(prog) /usr/$(prog);)


tests: bin/tests magic
	@echo "Building tests..."
	@for source in $(TST_SRC); do\
		($(CC) $(INC)  $(CFLAGS) $(MASSEUSE_SRC) src/tests/$$source.c  -o bin/tests/$${source%.*}.bin $(LINK)) || (exit 1);\
	done

test: tests
	@./test_runner.py

clean:
	make -C src/libnn clean
	make -C src/seen clean
	@rm -rf obj
	@rm -rf bin
