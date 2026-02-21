#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

int rows, cols;
int space_between_ascii_and_info = 1;
bool auto_shorten = true;

#include "util.h"
#include "ascii.h"

void print_ascii(char *str) {
	form_info_list();
	
	int term_width = get_term_width();
	if (!auto_shorten) term_width = 99999;

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
	
	int current_ascii_width = 0; 
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
					int visual_len = 0;
					bool dots_printed = false;

					print_clipped(ascii_color, line_buffer, &visual_len, term_width, &dots_printed);
					current_ascii_width += utf8_strlen(line_buffer, false);
					line_buffer[0] = '\0';

					int padding = max_width - current_ascii_width + space_between_ascii_and_info;
					for (int k = 0; k < padding; k++) {
						print_clipped(NULL, " ", &visual_len, term_width, &dots_printed);
					}

					bool line_complete = false;
					while (info_line < info_count && !line_complete) {
						if (info_list[info_line] != NULL && check_hex(info_list[info_line])) {
							char clean_hex[64];
							sscanf(info_list[info_line], "%s", clean_hex);
							strcpy(info_color, clean_hex);
							info_line++;
							continue;
						}

						if (info_list[info_line] != NULL) {
							if (check_end_newline(info_list[info_line])) {
								char clean_str[1024];
								copy_without_last_two(clean_str, info_list[info_line]);
								print_clipped(info_color, clean_str, &visual_len, term_width, &dots_printed);
								line_complete = true;
							} else {
								print_clipped(info_color, info_list[info_line], &visual_len, term_width, &dots_printed);
							}
							info_line++;
						} else {
							line_complete = true;
						}
					}

					printf("\n");
					current_ascii_width = 0;
				} else {
					temp_char[0] = *ptr;
					strcat(line_buffer, temp_char);
				}
				ptr++;
			}
		}
	}

	while (info_line < info_count) {
		int visual_len = 0;
		bool dots_printed = false;

		int padding = max_width + space_between_ascii_and_info;
		for (int k = 0; k < padding; k++) {
			print_clipped(NULL, " ", &visual_len, term_width, &dots_printed);
		}

		bool line_complete = false;
		while (info_line < info_count && !line_complete) {
			if (info_list[info_line] != NULL && check_hex(info_list[info_line])) {
				char clean_hex[64];
				sscanf(info_list[info_line], "%s", clean_hex);
				strcpy(info_color, clean_hex);
				info_line++;
				continue;
			}

			if (info_list[info_line] != NULL) {
				if (check_end_newline(info_list[info_line])) {
					char clean_str[1024];
					copy_without_last_two(clean_str, info_list[info_line]);
					print_clipped(info_color, clean_str, &visual_len, term_width, &dots_printed);
					line_complete = true;
				} else {
					print_clipped(info_color, info_list[info_line], &visual_len, term_width, &dots_printed);
				}
				info_line++;
			} else {
				line_complete = true;
			}
		}
		printf("\n");
	}

	free_info_list();
}

int main() {
	print_ascii("arch_linux-default");
	return 0;
}