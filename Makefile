.PHONY: all clean obj install

CXX = g++
CFLAGS = -g -Wall -Wextra -O2 -std=c++11

SOURCES=$(wildcard src/*.cc)
OBJECTS=$(addprefix obj/,$(notdir $(SOURCES:.cc=.o)))
TARGET = librtp.a

all: $(TARGET)

install: $(TARGET)
	install -m 577 $(TARGET) /usr/local/lib/

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

obj/%.o: src/%.cc | obj
	$(CXX) $(CFLAGS) -c -o $@ $<

obj:
	@mkdir -p $@

clean:
	rm -rf obj $(TARGET)
