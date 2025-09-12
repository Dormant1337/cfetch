#include <ctype.h> 
#include <stddef.h> 
#include <stdlib.h>
#include <string.h>

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