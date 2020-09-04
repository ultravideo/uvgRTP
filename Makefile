.PHONY: all clean obj install

CXX = g++
CXXFLAGS = -g -Wno-unused-function -Wall -Wextra -Wuninitialized -O2 -std=c++11 -Iinclude -fPIC -DNDEBUG
SOURCES = $(wildcard src/*.cc)
MODULES := src/formats src/zrtp src/rtcp src/srtp
-include $(patsubst %, %/module.mk, $(MODULES))
OBJECTS := $(patsubst %.cc, %.o, $(filter %.cc, $(SOURCES)))

TARGET = libuvgrtp.a

all: $(TARGET)

install: $(TARGET)
	install -m 577 $(TARGET) /usr/local/lib/
	mkdir -p /usr/local/include/uvgrtp /usr/local/include/uvgrtp/formats
	mkdir -p /usr/local/include/uvgrtp/zrtp /usr/local/include/uvgrtp/srtp
	cp include/*.hh /usr/local/include/uvgrtp
	cp include/formats/*.hh /usr/local/include/uvgrtp/formats
	cp include/zrtp/*.hh /usr/local/include/uvgrtp/zrtp
	cp include/srtp/*.hh /usr/local/include/uvgrtp/srtp

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

clean:
	rm -f src/*.o src/formats/*.o src/zrtp/*.o src/srtp/*.o $(TARGET)
