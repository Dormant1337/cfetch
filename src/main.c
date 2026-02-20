#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

int rows, cols;
int space_between_ascii_and_info = 1;

#include "util.h"
#include "ascii.h"

void print_ascii(char *str) {
	form_info_list();
	
	int max_width = 0;
	if (strcmp(str, "arch_linux-default") == 0) {
		int current_w = 0;
		for (int i = 0; arch_linux_default[i] != NULL; i++) {
			if (check_hex(arch_linux_default[i])) continue;
			for (int j = 0; arch_linux_default[i][j] != '\0'; j++) {
				if (arch_linux_default[i][j] == '\n') {
					if (current_w > max_width) max_width = current_w;
					current_w = 0;
				} else {
					current_w++;
				}
			}
		}
		if (current_w > max_width) max_width = current_w;
	}

	int info_line = 0;
	char ascii_color[64] = "#ffffff";
	char info_color[64] = "#ffffff";
	int current_line_width = 0;
	char line_buffer[1024];
	line_buffer[0] = '\0';

	if (strcmp(str, "arch_linux-default") == 0) {
		for (int i = 0; arch_linux_default[i] != NULL; i++) {
			if (check_hex(arch_linux_default[i])) {
				char clean_hex[64];
				sscanf(arch_linux_default[i], "%s", clean_hex);
				strcpy(ascii_color, clean_hex);
				continue;
			}

			const char *ptr = arch_linux_default[i];
			char temp_char[2] = {0, 0};

			while (*ptr) {
				if (*ptr == '\n') {
					hex_printf(ascii_color, "%s", line_buffer);
					current_line_width += utf8_strlen(line_buffer, false);
					line_buffer[0] = '\0';

					int padding = max_width - current_line_width + space_between_ascii_and_info;
					for (int k = 0; k < padding; k++) printf(" ");

					while (info_line < info_count && info_list[info_line] != NULL && check_hex(info_list[info_line])) {
						char clean_hex[64];
						sscanf(info_list[info_line], "%s", clean_hex);
						strcpy(info_color, clean_hex);
						info_line++;
					}

					if (info_line < info_count && info_list[info_line] != NULL) {
						hex_printf(info_color, "%s", info_list[info_line]);
						info_line++;
					}

					printf("\n");
					current_line_width = 0;
				} else {
					temp_char[0] = *ptr;
					strcat(line_buffer, temp_char);
				}
				ptr++;
			}
			
			if (line_buffer[0] != '\0') {
				hex_printf(ascii_color, "%s", line_buffer);
				current_line_width += utf8_strlen(line_buffer, false);
				line_buffer[0] = '\0';
			}
		}
	}

	while (info_line < info_count) {
		int padding = max_width + space_between_ascii_and_info;
		for (int k = 0; k < padding; k++) printf(" ");

		while (info_line < info_count && info_list[info_line] != NULL && check_hex(info_list[info_line])) {
			char clean_hex[64];
			sscanf(info_list[info_line], "%s", clean_hex);
			strcpy(info_color, clean_hex);
			info_line++;
		}

		if (info_line < info_count && info_list[info_line] != NULL) {
			hex_printf(info_color, "%s", info_list[info_line]);
			info_line++;
		}
		printf("\n");
	}

	free_info_list();
}

int main() {
	print_ascii("arch_linux-default");
	return 0;
}