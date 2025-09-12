

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
void capitalize_first(char *s);
char *xstrdup(const char *s);
char *str_tolower_dup(const char *s);
char *ltrim(char *s);
void rtrim_inplace(char *s);
void strip_inline_comment(char *s);
char *read_kv_value(char *s);
int parse_line_index(const char *s, int *out_idx);
char *escape_c_string(const char *input);
size_t strlen_safe(const char *s);
#endif