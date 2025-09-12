CC = gcc

CFLAGS = -g -Wall

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin


TARGET = cfetch

SOURCES = main.c fetch_hw.c fetch_sw.c utils.c

OBJECTS = $(SOURCES:.c=.o)


all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: all
	@echo "Installing cfetch to $(BINDIR)..."
	@mkdir -p $(BINDIR)
	@install -m 0755 $(TARGET) $(BINDIR)
	@echo "Installation complete."

uninstall:
	@echo "Removing cfetch from $(BINDIR)..."
	@rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstallation complete."