CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g -Isrc
LDFLAGS ?=

SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c, build/%.o, $(SRC))
DEPS := $(OBJ:.o=.d)

TARGET ?= cfetch

.PHONY: all clean distclean format

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	rm -rf build $(TARGET)

distclean: clean
	rm -f *~ .depend

format:
	@find src -name '*.c' -o -name '*.h' | xargs -r clang-format -i