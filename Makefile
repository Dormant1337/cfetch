CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g -Isrc

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c, build/%.o, $(SRC))
DEPS := $(OBJ:.o=.d)

TARGET ?= cfetch

.PHONY: all clean distclean format

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

-include $(DEPS)

clean:
	rm -rf build $(TARGET)

distclean: clean
	rm -f *~ .depend

format:
	clang-format -i src/*.c src/*.h

