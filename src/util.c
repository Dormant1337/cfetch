#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "ascii.h"

int utf8_strlen(const char *s, bool ignore_spaces) {
	int count = 0;
	while (*s) {
		if ((*s & 0xC0) != 0x80) {
			if (!(ignore_spaces && *s == ' ')) {
				count++;
			}
		}
		s++;
	}
	return count;
}

void hex_printf(const char *hex, const char *format, ...) {
	unsigned int r, g, b;

	if (hex[0] == '#') {
                hex++;
	}

	if (sscanf(hex, "%02x%02x%02x", &r, &g, &b) != 3) {
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		return;
	}

	printf("\033[38;2;%u;%u;%um", r, g, b);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\033[0m");
}

void get_lengths_of_ascii(int *lenest_spaceless, int *lenest_space, char *str) {
        if (strcmp(str, "arch_linux-default") == 0) {
                for(int i = 0; arch_linux_default[i] != NULL; i++) {
                        if (utf8_strlen(arch_linux_default[i], true) > *lenest_spaceless) {
                                *lenest_spaceless = utf8_strlen(arch_linux_default[i], true);
                        }
                        if (utf8_strlen(arch_linux_default[i], false) > *lenest_space) {
                                *lenest_space = utf8_strlen(arch_linux_default[i], false);
                        }
                }
        }
}

int check_hex(char *str) {
	if (str[0] != '#') return 0;

	size_t len = strlen(str);
	
	if (len > 0 && str[len - 1] == '\n') len--;

	if (len == 7 || len == 4) {
		return 1;
	}
	return 0;
}