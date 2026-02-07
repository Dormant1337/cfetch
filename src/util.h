#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

extern char **info_list;
extern int info_count;

int utf8_strlen(const char *s, bool ignore_spaces);
void hex_printf(const char *hex, const char *format, ...);
void get_lengths_of_ascii(int *lenest_spaceless, int *lenest_space, const char *str);
int check_hex(const char *str);
void add_info_line(const char *fmt, ...);
void free_info_list(void);
void form_info_list(void);
int check_end_newline(const char *str);
void copy_without_last_two(char *dest, const char *src);


#endif /* UTIL_H */