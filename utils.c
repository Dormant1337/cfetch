#include <ctype.h> 
#include <stddef.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Files includes. */
#include "utils.h"

void capitalize_first(char *s)
{
	if (s && s[0] != '\0') {
		s[0] = (char)toupper((unsigned char)s[0]);
	}
}

char *str_tolower_dup(const char *s) {
	if (!s) return NULL;
	size_t n = strlen(s);
	char *p = malloc(n + 1);
	if (!p) return NULL;
	for (size_t i = 0; i < n; i++) {
		p[i] = (char)tolower((unsigned char)s[i]);
	}
	p[n] = '\0';
	return p;
}

char *ltrim(char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	return s;
}

void rtrim_inplace(char *s)
{
	size_t n;

	if (!s)
		return;
	n = strlen(s);
	while (n > 0 && isspace((unsigned char)s[n - 1])) {
		s[n - 1] = '\0';
		n--;
	}
}

void strip_inline_comment(char *s)
{
	int inq = 0;
	char *p = s;

	while (*p) {
		if (*p == '"')
			inq = !inq;
		else if (*p == '#' && !inq) {
			*p = '\0';
			break;
		}
		p++;
	}
}

char *read_kv_value(char *s)
{
	char *eq;
	char *v;

	eq = strchr(s, '=');
	if (!eq)
		return NULL;
	v = eq + 1;
	v = ltrim(v);
	if (*v == '"') {
		char *end = strrchr(v + 1, '"');
		if (!end)
			return NULL;
		*end = '\0';
		return v + 1;
	}
	return v;
}

int parse_line_index(const char *s, int *out_idx)
{
	const char *p;
	char *end;
	long v;

	if (strncmp(s, "line[", 5) != 0)
		return -1;
	p = s + 5;
	v = strtol(p, &end, 10);
	if (end == p)
		return -1;
	if (*end != ']')
		return -1;
	*out_idx = (int)v;
	return 0;
}

char *escape_c_string(const char *input)
{
	size_t len = strlen(input);
	char *escaped = malloc(len * 2 + 1);
	char *writer;
	size_t i;

	if (!escaped) {
		perror("Memory allocation error for escaped string");
		return NULL;
	}

	writer = escaped;
	for (i = 0; i < len; ++i) {
		switch (input[i]) {
		case '\"':
			*writer++ = '\\';
			*writer++ = '\"';
			break;
		case '\\':
			*writer++ = '\\';
			*writer++ = '\\';
			break;
		default:
			*writer++ = input[i];
			break;
		}
	}
	*writer = '\0';
	return escaped;
}

size_t strlen_safe(const char *s)
{
	if (!s)
		return 0;
	return strlen(s);
}