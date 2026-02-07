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
	char spacer_char[space_between_ascii_and_info + 1];

	if (space_between_ascii_and_info > 0) {
		for (int i = 0; i < space_between_ascii_and_info; i++) spacer_char[i] = ' ';
		spacer_char[space_between_ascii_and_info] = '\0';
	}

	int lenest_spaceless = 0, lenest_space = 0;
	get_lengths_of_ascii(&lenest_spaceless, &lenest_space, str);
	int len_est = lenest_space;

	if (strcmp(str, "arch_linux-default") == 0) {
		for (int i = 0; arch_linux_default[i] != NULL; i++) {
			if (check_hex(arch_linux_default[i])) {
				strcpy(saved_hex, arch_linux_default[i]);
				continue;
			}

			char temp_str[512];
			if (check_end_newline(arch_linux_default[i])) {
				copy_without_last_two(temp_str, arch_linux_default[i]);
				hex_printf(saved_hex, "%s", temp_str);
				current_line_width += utf8_strlen(temp_str, false);

				for (int j = 0; j < (len_est - current_line_width); j++) printf(" ");
				
				printf("%s", spacer_char);

				if (line_count < info_count) {
					hex_printf(saved_hex_info, "%s", info_list[line_count]);
				}

				printf("\n");
				line_count++;
				current_line_width = 0;
			} else {
				hex_printf(saved_hex, "%s", arch_linux_default[i]);
				current_line_width += utf8_strlen(arch_linux_default[i], false);
			}
		}
	}
	free_info_list();
}

int main() {
        print_ascii("arch_linux-default");
        return 0;
}