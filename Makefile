.PHONY: all clean obj install

CXX = g++
CXXFLAGS = -g -Wall -Wextra -Wuninitialized -O2 -std=c++11 -Iinclude -fPIC -DNDEBUG
SOURCES = $(wildcard src/*.cc)
MODULES := src/formats src/mzrtp
-include $(patsubst %, %/module.mk, $(MODULES))
OBJECTS := $(patsubst %.cc, %.o, $(filter %.cc, $(SOURCES)))

TARGET = libuvgrtp.a

all: $(TARGET)

install: $(TARGET)
	install -m 577 $(TARGET) /usr/local/lib/
	mkdir -p /usr/local/include/uvgrtp /usr/local/include/uvgrtp/formats /usr/local/include/uvgrtp/mzrtp
	cp include/*.hh /usr/local/include/uvgrtp
	cp include/formats/*.hh /usr/local/include/uvgrtp/formats
	cp include/mzrtp/*.hh /usr/local/include/uvgrtp/mzrtp

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

clean:
	rm -f src/*.o src/formats/*.o src/mzrtp/*.o $(TARGET)
