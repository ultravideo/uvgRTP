.PHONY: all clean obj install

CXX = g++
CFLAGS = -g -Wall -Wextra -O2 -std=c++11 -DNDEBUG

SOURCES=$(wildcard src/*.cc)
OBJECTS=$(addprefix obj/,$(notdir $(SOURCES:.cc=.o)))
TARGET = libkvzrtp.a

all: $(TARGET)

install: $(TARGET)
	install -m 577 $(TARGET) /usr/local/lib/
	mkdir -p /usr/local/include/kvzrtp
	cp src/*.hh /usr/local/include/kvzrtp

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

obj/%.o: src/%.cc | obj
	$(CXX) $(CFLAGS) -c -o $@ $<

obj:
	@mkdir -p $@

clean:
	rm -rf obj $(TARGET)
