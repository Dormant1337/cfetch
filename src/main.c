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
	int line_count = 0;
	int current_line_width = 0;
	char saved_hex[64] = "#ffffff";
	char saved_hex_info[64] = "#ffffff";

	int lenest_spaceless = 0, lenest_space = 0;
	get_lengths_of_ascii(&lenest_spaceless, &lenest_space, str);
	int len_est = lenest_space;

	if (strcmp(str, "arch_linux-default") == 0) {
		for (int i = 0; arch_linux_default[i] != NULL; i++) {
			if (check_hex(arch_linux_default[i])) {
				char clean_hex[64];
				sscanf(arch_linux_default[i], "%s", clean_hex);
				strcpy(saved_hex, clean_hex);
				continue;
			}

			char temp_str[512];
			if (check_end_newline(arch_linux_default[i])) {
				copy_without_last_two(temp_str, arch_linux_default[i]);
				hex_printf(saved_hex, "%s", temp_str);
				current_line_width += utf8_strlen(temp_str, false);

				int padding = len_est - current_line_width;
				for (int j = 0; j < padding; j++) printf(" ");
				for (int j = 0; j < space_between_ascii_and_info; j++) printf(" ");

				while (line_count < info_count && info_list[line_count] != NULL && check_hex(info_list[line_count])) {
					char clean_hex[64];
					sscanf(info_list[line_count], "%s", clean_hex);
					strcpy(saved_hex_info, clean_hex);
					line_count++;
				}

				if (line_count < info_count && info_list[line_count] != NULL) {
					hex_printf(saved_hex_info, "%s", info_list[line_count]);
					line_count++;
				}

				printf("\n");
				current_line_width = 0;
			} else {
				hex_printf(saved_hex, "%s", arch_linux_default[i]);
				current_line_width += utf8_strlen(arch_linux_default[i], false);
			}
		}
	}

	while (line_count < info_count) {
		for (int j = 0; j < len_est + space_between_ascii_and_info; j++) printf(" ");

		while (line_count < info_count && info_list[line_count] != NULL && check_hex(info_list[line_count])) {
			char clean_hex[64];
			sscanf(info_list[line_count], "%s", clean_hex);
			strcpy(saved_hex_info, clean_hex);
			line_count++;
		}

		if (line_count < info_count && info_list[line_count] != NULL) {
			hex_printf(saved_hex_info, "%s", info_list[line_count]);
			line_count++;
		}
		printf("\n");
	}

	free_info_list();
}

int main() {
        print_ascii("arch_linux-default");
        return 0;
}