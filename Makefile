$(eval OS := $(shell uname))

NN=src/libnn

CC=gcc
CXX=clang++
CFLAGS=-g --std=c99 -D_XOPEN_SOURCE=500 -Wall -Wno-implicit-function-declaration
CXXFLAGS=--std=c++11 -g
INC=-Isrc -I$(NN)/src -Isrc/drivers -Isrc/drivers/src/BNO055_driver -Isrc/linmath.h -Isrc/seen/src -Isrc/json -Iml/recognizer/src
LINK=-lm -lpthread

TARGET=$(shell $(CC) -dumpmachine)

DRIVER_SRC= drivers/BNO055_driver/bno055.c drivers/BNO055_driver/bno055_support.c drivers/drv_pwm.c i2c.c
BASE_SRC = sys.c $(DRIVER_SRC)

COLLECTOR_SRC=deadreckon.c collector.c cam.c $(BASE_SRC)
PREDICTOR_FLAGS=-funsafe-math-optimizations -march=native -O3 -ftree-vectorize
PREDICTOR_SRC=predictor.c $(BASE_SRC)
PREDICTOR_LINK=$(NN)/build/$(TARGET)/lib/libnn.a
ACTUATOR_SRC=actuator.c $(BASE_SRC)

VIEWER_SRC=sys.c viewer.c
VIEWER_LINK=
BOTD_SRC=sys.c botd.c $(DRIVER_SRC)
TST_SRC=masseuse_falloff masseuse_bucket

SIM_SRC=src/sim/sim.cpp src/sys.c
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

bin/$(TARGET):
	mkdir -p bin/$(TARGET)
	cp actions.cal bin/$(TARGET)

external:


bin/$(TARGET)/data: bin/$(TARGET)
	ln -sf $(shell pwd)/src/seen/demos/data/ bin/$(TARGET)/data

bin/$(TARGET)/scene.json: bin/$(TARGET)
	cp .scene.json bin/$(TARGET)/scene.json

obj/%.o: src/%.c magic obj
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) -c $< -o $@

all: viewer collector masseuse

$(NN):
	git clone https://github.com/mrpossoms/libnn $(NN)

$(NN)/lib/libnn.a: $(NN)
	make -C $(NN) static

src/json:
	mkdir -p src/json
	wget https://raw.githubusercontent.com/nlohmann/json/master/single_include/nlohmann/json.hpp --output-document src/json/json.hpp

src/linmath.h:
	git clone https://github.com/mrpossoms/linmath.h src/linmath.h
	make -C src/linmath.h install

src/seen:
	git clone https://github.com/mrpossoms/Seen src/seen


src/seen/lib/libseen.a: src/seen
	make -C src/seen static

magic: src/structs.h src/linmath.h $(NN)/lib/libnn.a src/seen src/json bin/$(TARGET)/data bin/$(TARGET)/scene.json
	cksum src/structs.h | awk '{split($$0,a," "); print a[1]}' > magic

.PHONY: collector
collector:bin/$(TARGET)/collector
	@echo -e "\e[92mBuilt collector for" $(TARGET)
bin/$(TARGET)/collector: $(addprefix obj/,$(COLLECTOR_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

.PHONY: predictor
predictor:bin/$(TARGET)/predictor
	@echo -e "\e[92mBuilt predictor for" $(TARGET)
bin/$(TARGET)/predictor: $(addprefix obj/,$(PREDICTOR_SRC:.c=.o))
	$(CC) $(CFLAGS) $(PREDICTOR_FLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(PREDICTOR_LINK) $(LINK)

.PHONY: actuator
actuator:bin/$(TARGET)/actuator
	@echo -e "\e[92mBuilt actuator for" $(TARGET)
bin/$(TARGET)/actuator: $(addprefix obj/,$(ACTUATOR_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

.PHONY: botd
botd:bin/$(TARGET)/botd
	@echo -e "\e[92mBuilt botd for" $(TARGET)
bin/$(TARGET)/botd: $(addprefix obj/,$(BOTD_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK)

.PHONY: viewer
viewer:bin/$(TARGET)/viewer
	@echo -e "\e[92mBuilt viewer for" $(TARGET)
bin/$(TARGET)/viewer: $(addprefix obj/,$(VIEWER_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(LIB_PATHS) $(INC) $(LIB_INC) $^ -o $@ $(VIEWER_LINK) $(LINK)

.PHONY: sim
sim:bin/$(TARGET)/sim
	@echo -e "\e[92mBuilt sim for" $(TARGET)
bin/$(TARGET)/sim: magic src/seen/lib/libseen.a
	$(CXX) $(CXXFLAGS) -DMAGIC=$(shell cat magic) $(LIB_PATHS) $(INC) $(LIB_INC) $(SIM_INC) $(SIM_SRC) -o $@ -lpng src/seen/lib/libseen.a $(VIEWER_LINK) $(LINK)

.PHONY: trainx
trainx:bin/$(TARGET)/trainx
	@echo -e "\e[92mBuilt trainx for" $(TARGET)
bin/$(TARGET)/trainx: $(addprefix obj/,$(TRAINX_SRC:.c=.o))
	$(CC) $(CFLAGS) -DMAGIC=$(shell cat magic) $(INC) $^ -o $@ $(LINK) -lpng -lz

bot-utils: bin/$(TARGET)/predictor bin/$(TARGET)/actuator bin/$(TARGET)/collector bin/$(TARGET)/trainx bin/$(TARGET)/botd
	@echo -e "\e[92mBuilt bot utilities"

install-bot: bot-utils
	$(foreach prog, $^, ln -s $(shell pwd)/$(prog) /usr/$(prog);)

install-tools: bin/$(TARGET)/viewer bin/$(TARGET)/sim
	$(foreach prog, $^, ln -s $(shell pwd)/$(prog) /usr/$(prog);)


tests: bin/$(TARGET)/tests magic
	@echo -e "\e[92mBuilding tests..."
	@for source in $(TST_SRC); do\
		($(CC) $(INC)  $(CFLAGS) $(MASSEUSE_SRC) src/tests/$$source.c  -o bin/$(TARGET)/tests/$${source%.*}.bin $(LINK)) || (exit 1);\
	done

test: tests
	@./test_runner.py

clean:
	make -C $(NN) clean
	make -C src/seen clean
	@rm -rf obj
	@rm -rf bin/$(TARGET)
