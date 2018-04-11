$(eval OS := $(shell uname))

CXX=g++
CFLAGS=--std=c++11 -g
INC=-I/usr/local/include -L/usr/local/lib
LINK= /usr/local/lib/libseen.a -lode -lpng

ifeq ($(OS),Darwin)
	LINK +=-lpthread -lm -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
	LINK += -lopencv_videoio
else
	LINK +=-lglfw3 -lGL -lX11 -lXi -lXrandr -lXxf86vm -lXinerama -lXcursor -lrt -lm -pthread -ldl
	CFLAGS += -D_XOPEN_SOURCE=500 -D_GNU_SOURCE -DGL_GLEXT_PROTOTYPES
endif

hay: src/hay.cpp
	$(CXX) $(CFLAGS) $(INC) src/$@.cpp -o $@ $(LINK)

asphalt: src/asphalt.cpp
	$(CXX) $(CFLAGS) $(INC) src/$@.cpp -o $@ $(LINK)

clean:
	rm hay asphalt
