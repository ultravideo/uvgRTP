.PHONY: all clean obj

CXX = g++
CFLAGS = -g -Wall -Wextra -O2 -std=c++11

SOURCES=$(wildcard src/*.cc)
OBJECTS=$(addprefix obj/,$(notdir $(SOURCES:.cc=.o)))
TARGET = librtp.a

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

obj/%.o: src/%.cc | obj
	$(CXX) $(CFLAGS) -c -o $@ $<

obj:
	@mkdir -p $@

clean:
	rm -rf obj $(TARGET)
