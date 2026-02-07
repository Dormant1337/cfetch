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
        int line_count = 0;
        char saved_hex[64] = "#ffffff";
        char spacer_char[space_between_ascii_and_info + 1];
        if (space_between_ascii_and_info > 0) {
                for (int i = 0; i < space_between_ascii_and_info; i++) {
                        spacer_char[i] = ' ';
                }
                spacer_char[space_between_ascii_and_info] = '\0';
        }



        int temp_len = 0;
        int len_est = 0; // init variable that contains the length of the longEST string.
        int lenest_spaceless = 0;
        int lenest_space = 0;
        get_lengths_of_ascii(&lenest_spaceless, &lenest_space, str);
        len_est = lenest_space;
        if (strcmp(str, "arch_linux-default") == 0) {
                
                for(int i = 0; arch_linux_default[i] != NULL; i++) {
                        if (!check_hex(arch_linux_default[i])) {     

                                temp_len = utf8_strlen(arch_linux_default[i], false);
                                hex_printf(saved_hex, "%s", arch_linux_default[i]);
                                
                                if (temp_len < len_est) {
                                        for (int j = 0; j < len_est - temp_len; j++) {
                                                hex_printf(saved_hex, "%s", spacer_char);
                                        }
                                }
                                hex_printf(saved_hex, "%s", spacer_char);
                                line_count++;
                                puts("1");
                        } else {
                                strcpy(saved_hex, arch_linux_default[i]);
                        }
                }
                
                
        }
}

int main() {
        print_ascii("arch_linux-default");
        return 0;
}