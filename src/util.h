#ifndef UTIL_H
#define UTIL_H

int utf8_strlen(const char *s, bool ignore_spaces);
void hex_printf(const char *hex, const char *format, ...);
void get_lengths_of_ascii(int *lenest_spaceless, int *lenest_space, char *str);
int check_hex(char *str);

#endif /* UTIL_H */
