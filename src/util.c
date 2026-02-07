#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

char **info_list = NULL;
int info_count = 0;


#include "ascii.h"
#include "util.h"
#include "fetch/hw.h"



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

void get_lengths_of_ascii(int *lenest_spaceless, int *lenest_space, const char *str) {
	if (strcmp(str, "arch_linux-default") == 0) {
		for(int i = 0; arch_linux_default[i] != NULL; i++) {
			if (check_hex(arch_linux_default[i])) continue;

			if (utf8_strlen(arch_linux_default[i], true) > *lenest_spaceless) {
				*lenest_spaceless = utf8_strlen(arch_linux_default[i], true);
			}
			if (utf8_strlen(arch_linux_default[i], false) > *lenest_space) {
				*lenest_space = utf8_strlen(arch_linux_default[i], false);
			}
		}
	}
}

int check_hex(const char *str) {
	if (str[0] != '#') return 0;

	size_t len = strlen(str);
	
	if (len > 0 && str[len - 1] == '\n') len--;

	if (len == 7 || len == 4) {
		return 1;
	}
	return 0;
}

void free_info_list(void) {
	for (int i = 0; i < info_count; i++) {
		free(info_list[i]);
	}
	free(info_list);
	info_list = NULL;
	info_count = 0;
}

void add_info_line(const char *fmt, ...) {
	va_list args;
	char buffer[512];

	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	char **tmp = realloc(info_list, (info_count + 1) * sizeof(char *));
	if (tmp == NULL) {
		return;
	}
	info_list = tmp;

	info_list[info_count] = strdup(buffer);
	info_count++;
}

void form_info_list() {
        char gpu_name[256];
        char cpu_name[256];
        
        int gpu_count = get_gpu_count();
        int cpu_count = get_cpu_count();

        for (int i = 0; i < gpu_count; i++) {
                get_gpu_name(gpu_name, i);
                add_info_line("GPU: %s", gpu_name);
        }

        for (int i = 0; i < cpu_count; i++) {
                get_cpu_name(cpu_name, i);
                add_info_line("CPU: %s", cpu_name);
        }

        
}