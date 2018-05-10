$(eval OS := $(shell uname))

CC=gcc
CXX=g++
CFLAGS=-g --std=c99 -D_XOPEN_SOURCE=500
CXXFLAGS=--std=c++11 -g
DRIVER_SRC= drivers/BNO055_driver/bno055.c drivers/BNO055_driver/bno055_support.c drivers/drv_pwm.c i2c.c
COLLECTOR_SRC=deadreckon.c sys.c collector.c cam.c $(DRIVER_SRC)
PREDICTOR_FLAGS=-funsafe-math-optimizations -march=native -O3 -ftree-vectorize
PREDICTOR_SRC=predictor.c sys.c $(DRIVER_SRC)
PREDICTOR_LINK=src/nn.h/lib/libnn.a
ACTUATOR_SRC=actuator.c sys.c i2c.c drv_pwm.c $(DRIVER_SRC)
INC=-Isrc -Isrc/nn.h/src -Isrc/drivers -Isrc/drivers/src/BNO055_driver -Isrc/linmath -Isrc/seen/src -Isrc/json -Iml/recognizer/src
LINK=-lm -lpthread
VIEWER_SRC=sys.c viewer.c
VIEWER_LINK=
BOTD_SRC=sys.c i2c.c drv_pwm.c BNO055_driver/bno055.c BNO055_driver/bno055_support.c botd.c
TST_SRC=masseuse_falloff masseuse_bucket

SIM_SRC=src/sim/sim.cpp src/sys.c src/seen/demos/src/sky.cpp
SIM_INC=-Isrc/seen/demos/src/

RECOG_SRC=nn.c recognizer.c sys.c
RECOG_INC=-Iml/recognizer/src

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
	cp actions.cal bin/

bin/data: bin
	ln -sf $(shell pwd)/src/seen/demos/data/ bin/data

bin/scene.json: bin
	cp .scene.json bin/scene.json

obj/%.o: src/%.c magic obj
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) -c $< -o $@

all: viewer collector masseuse

src/nn.h:
	git clone https://github.com/mrpossoms/nn.h src/nn.h
	make -C src/nn.h static

src/json:
	mkdir -p src/json
	wget https://raw.githubusercontent.com/nlohmann/json/master/single_include/nlohmann/json.hpp --output-document src/json/json.hpp

src/linmath.h:
	git clone https://github.com/mrpossoms/linmath.h src/linmath.h
	make -C src/linmath.h install

src/seen:
	git clone https://github.com/mrpossoms/Seen src/seen
	make -C src/seen static

magic: src/structs.h src/linmath.h src/nn.h src/seen src/json bin/data bin/scene.json
	cksum src/structs.h | awk '{split($$0,a," "); print a[1]}' > magic

bin/structsize: bin
	$(CC) $(CFLAGS) $(INC) src/size.c -o structsize

bin/viewer: $(addprefix obj/,$(VIEWER_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) -L/usr/local/lib $(INC) $^ -o $@ $(VIEWER_LINK) $(LINK)

bin/collector: $(addprefix obj/,$(COLLECTOR_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/predictor: $(addprefix obj/,$(PREDICTOR_SRC:.c=.o))
	$(CC) $(CFLAGS) $(PREDICTOR_FLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK) $(PREDICTOR_LINK)

bin/actuator: $(addprefix obj/,$(ACTUATOR_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/botd: $(addprefix obj/,$(BOTD_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

bin/sim: magic
	$(CXX) $(CXXFLAGS) -DMAGIC=$(shell cat magic) $(INC) $(SIM_INC) $(SIM_SRC) -o $@ $(VIEWER_LINK) $(LINK) -lpng src/seen/lib/libseen.a

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
	@rm -rf bin
