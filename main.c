#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ascii.c"
#include <glob.h>
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <time.h>
#include <sys/statvfs.h>
#include "fetch_hw.h"
#include "fetch_sw.h"
#include "utils.h"

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

#define MAX_ASCII_FILE_LINES            1000
#define MAX_ASCII_FILE_LINE_LENGTH      512

char last_hexcolor[16];

struct kv_pair {
	char *key;
	char *val;
};

struct line_cfg {
	int present;
	char *format;
	char *color;
	char *label_color;
	char *data_color;
	struct kv_pair *forced;
	size_t forced_count;
	int arrange_box;
};

struct cfetch_cfg {
	char *ascii_art;
	char *default_label_color;
	char *default_data_color;
	int info_padding;
	struct line_cfg *lines;
	size_t lines_count;
	char *frame_type;
	char *frame_color;
	char *frame_title_soft;
	char *frame_title_hard;
};



static char *ltrim(char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	return s;
}

static void rtrim_inplace(char *s)
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

static void strip_inline_comment(char *s)
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

static char *read_kv_value(char *s)
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

static int parse_line_index(const char *s, int *out_idx)
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

static void cfg_init_defaults(struct cfetch_cfg *cfg)
{
	cfg->ascii_art = xstrdup("auto");
	cfg->default_label_color = xstrdup("#cccccc");
	cfg->default_data_color = xstrdup("#00ffff");
	cfg->info_padding = 4;
	cfg->lines = NULL;
	cfg->lines_count = 0;
	cfg->frame_type = xstrdup("none");
	cfg->frame_color = xstrdup("#cccccc");
	cfg->frame_title_soft = xstrdup("Softwares");
	cfg->frame_title_hard = xstrdup("Hardwares");
}

static void cfg_ensure_line(struct cfetch_cfg *cfg, size_t idx)
{
	size_t i;
	struct line_cfg *n;

	if (idx < cfg->lines_count)
		return;
	n = realloc(cfg->lines, sizeof(struct line_cfg) * (idx + 1));
	if (!n)
		return;
	for (i = cfg->lines_count; i <= idx; i++) {
		n[i].present = 0;
		n[i].format = NULL;
		n[i].color = NULL;
		n[i].label_color = NULL;
		n[i].data_color = NULL;
		n[i].forced = NULL;
		n[i].forced_count = 0;
		n[i].arrange_box = 0;
	}
	cfg->lines = n;
	cfg->lines_count = idx + 1;
}

static void cfg_free(struct cfetch_cfg *cfg)
{
	size_t i, j;

	free(cfg->ascii_art);
	free(cfg->default_label_color);
	free(cfg->default_data_color);
	free(cfg->frame_type);
	free(cfg->frame_color);
	free(cfg->frame_title_soft);
	free(cfg->frame_title_hard);
	for (i = 0; i < cfg->lines_count; i++) {
		free(cfg->lines[i].format);
		free(cfg->lines[i].color);
		free(cfg->lines[i].label_color);
		free(cfg->lines[i].data_color);
		for (j = 0; j < cfg->lines[i].forced_count; j++) {
			free(cfg->lines[i].forced[j].key);
			free(cfg->lines[i].forced[j].val);
		}
		free(cfg->lines[i].forced);
	}
	free(cfg->lines);
	memset(cfg, 0, sizeof(*cfg));
}

static void line_force_set(struct line_cfg *lc, const char *key, const char *val)
{
	size_t i;
	char *k;
	struct kv_pair *n;

	if (!lc || !key)
		return;
	k = str_tolower_dup(key);
	if (!k)
		return;
	for (i = 0; i < lc->forced_count; i++) {
		if (strcmp(lc->forced[i].key, k) == 0) {
			free(lc->forced[i].val);
			lc->forced[i].val = xstrdup(val ? val : "");
			free(k);
			return;
		}
	}
	n = realloc(lc->forced, sizeof(*n) * (lc->forced_count + 1));
	if (!n) {
		free(k);
		return;
	}
	lc->forced = n;
	lc->forced[lc->forced_count].key = k;
	lc->forced[lc->forced_count].val = xstrdup(val ? val : "");
	lc->forced_count++;
}

static char *line_force_get_dup(const struct line_cfg *lc, const char *key)
{
	size_t i;
	char *k;
	char *res = NULL;

	if (!lc || !key)
		return NULL;
	k = str_tolower_dup(key);
	if (!k)
		return NULL;
	for (i = 0; i < lc->forced_count; i++) {
		if (strcmp(lc->forced[i].key, k) == 0) {
			res = xstrdup(lc->forced[i].val ? lc->forced[i].val : "");
			break;
		}
	}
	free(k);
	return res;
}

static char *config_path(void)
{
	const char *xdg;
	const char *home;
	size_t n;
	char *p;

	xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && *xdg) {
		n = strlen(xdg) + strlen("/cfetch/config") + 1;
		p = malloc(n);
		if (!p)
			return NULL;
		snprintf(p, n, "%s/cfetch/config", xdg);
		return p;
	}
	home = getenv("HOME");
	if (!home || !*home)
		return NULL;
	n = strlen(home) + strlen("/.config/cfetch/config") + 1;
	p = malloc(n);
	if (!p)
		return NULL;
	snprintf(p, n, "%s/.config/cfetch/config", home);
	return p;
}

static int cfg_load(struct cfetch_cfg *cfg)
{
	char *path;
	FILE *f;
	char *line;
	size_t cap;
	ssize_t n;
	enum { SEC_NONE, SEC_GLOBAL, SEC_INFO, SEC_LINE } sec = SEC_NONE;
	int cur_line = -1;

	path = config_path();
	if (!path)
		return 0;
	f = fopen(path, "r");
	free(path);
	if (!f)
		return 0;

	line = NULL;
	cap = 0;
	while ((n = getline(&line, &cap, f)) != -1) {
		char *s = line;
		char *val;

		strip_inline_comment(s);
		s = ltrim(s);
		rtrim_inplace(s);
		if (*s == '\0')
			continue;

		if (strcmp(s, "global {") == 0) {
			sec = SEC_GLOBAL;
			continue;
		}
		if (strcmp(s, "info {") == 0) {
			sec = SEC_INFO;
			continue;
		}
		if (strncmp(s, "line[", 5) == 0 && sec == SEC_INFO) {
			int idx;
			const char *brace;

			if (parse_line_index(s, &idx) != 0)
				continue;
			brace = strchr(s, '{');
			if (!brace)
				continue;
			cur_line = idx;
			cfg_ensure_line(cfg, (size_t)idx);
			cfg->lines[idx].present = 1;
			sec = SEC_LINE;
			continue;
		}
		if (strcmp(s, "}") == 0) {
			if (sec == SEC_LINE) {
				sec = SEC_INFO;
				cur_line = -1;
			} else if (sec == SEC_GLOBAL || sec == SEC_INFO) {
				sec = SEC_NONE;
			}
			continue;
		}

		if (sec == SEC_GLOBAL) {
			if (strncmp(s, "ascii_art", 9) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(cfg->ascii_art);
					cfg->ascii_art = xstrdup(val);
				}
			} else if (strncmp(s, "default_label_color", 19) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(cfg->default_label_color);
					cfg->default_label_color = xstrdup(val);
				}
			} else if (strncmp(s, "default_data_color", 18) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(cfg->default_data_color);
					cfg->default_data_color = xstrdup(val);
				}
			} else if (strncmp(s, "info_padding", 12) == 0) {
				val = read_kv_value(s);
				if (val)
					cfg->info_padding = atoi(val);
			} else if (strncmp(s, "frame_type", 10) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(cfg->frame_type);
					cfg->frame_type = xstrdup(val);
				}
			} else if (strncmp(s, "frame_color", 11) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(cfg->frame_color);
					cfg->frame_color = xstrdup(val);
				}
			} else if (strncmp(s, "frame_title_soft", 16) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(cfg->frame_title_soft);
					cfg->frame_title_soft = xstrdup(val);
				}
			} else if (strncmp(s, "frame_title_hard", 16) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(cfg->frame_title_hard);
					cfg->frame_title_hard = xstrdup(val);
				}
			}
			continue;
		}
		if (sec == SEC_LINE && cur_line >= 0) {
			struct line_cfg *lc = &cfg->lines[cur_line];

			if (strncmp(s, "format", 6) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(lc->format);
					lc->format = xstrdup(val);
				}
			} else if (strncmp(s, "color", 5) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(lc->color);
					lc->color = xstrdup(val);
				}
			} else if (strncmp(s, "label_color", 11) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(lc->label_color);
					lc->label_color = xstrdup(val);
				}
			} else if (strncmp(s, "data_color", 10) == 0) {
				val = read_kv_value(s);
				if (val) {
					free(lc->data_color);
					lc->data_color = xstrdup(val);
				}
			} else if (strncmp(s, "force", 5) == 0) {
				char *p = s + 5;
				char *name = NULL;
				char *val2 = NULL;
				char *q;

				p = ltrim(p);
				if (*p == '%') {
					q = strchr(p + 1, '%');
					if (!q)
						goto done_force;
					name = strndup(p + 1, (size_t)(q - (p + 1)));
					p = q + 1;
				} else {
					q = p;
					while (*q && !isspace((unsigned char)*q) && *q != '=')
						q++;
					if (q == p)
						goto done_force;
					name = strndup(p, (size_t)(q - p));
					p = q;
				}

				p = ltrim(p);
				if (*p != '=')
					goto done_force;
				p++;
				p = ltrim(p);
				if (*p == '"') {
					q = strrchr(p + 1, '"');
					if (!q)
						goto done_force;
					*q = '\0';
					val2 = xstrdup(p + 1);
				} else {
					rtrim_inplace(p);
					val2 = xstrdup(p);
				}
				if (name && val2)
					line_force_set(lc, name, val2);
done_force:
				free(name);
				free(val2);
			} else if (strncmp(s, "arrange_box", 11) == 0) {
				val = read_kv_value(s);
				if (val)
					lc->arrange_box = atoi(val);
			}
			continue;
		}
	}
	free(line);
	fclose(f);
	return 1;
}












static char *escape_c_string(const char *input)
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

static int export_ascii_art(const char *filename)
{
	FILE *fp;
	char **art_lines;
	char buffer[MAX_ASCII_FILE_LINE_LENGTH];
	int line_count;
	int i;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("Error opening ASCII file");
		return 1;
	}

	art_lines = (char **)malloc(sizeof(char *) * MAX_ASCII_FILE_LINES);
	if (art_lines == NULL) {
		perror("Memory allocation error for art lines");
		fclose(fp);
		return 1;
	}

	line_count = 0;
	while (fgets(buffer, sizeof(buffer), fp) != NULL && line_count < MAX_ASCII_FILE_LINES) {
		buffer[strcspn(buffer, "\n")] = '\0';
		art_lines[line_count] = strdup(buffer);
		if (art_lines[line_count] == NULL) {
			perror("Memory allocation error for art line");
			for (i = 0; i < line_count; ++i)
				free(art_lines[i]);
			free(art_lines);
			fclose(fp);
			return 1;
		}
		line_count++;
	}
	fclose(fp);

	printf("const char *exported_ascii_art[] = {\n");
	for (i = 0; i < line_count; ++i) {
		char *escaped_line = escape_c_string(art_lines[i]);
		printf("        \"%s\",\n", escaped_line);
		free(escaped_line);
		free(art_lines[i]);
	}
	printf("        NULL\n");
	printf("};\n");

	free(art_lines);
	return 0;
}

int hex_to_true_color(const char *hex_color)
{
	long color_val;
	int r, g, b;
	const char *ptr = hex_color;
	char hex_part[7];
	size_t len_ptr;

	if (!hex_color)
		return -1;

	if (*ptr == '#')
		ptr++;
	
	len_ptr = strlen(ptr);
	if (len_ptr != 6 && len_ptr != 8)
		return -1;

	strncpy(hex_part, ptr, 6);
	hex_part[6] = '\0';

	color_val = strtol(hex_part, NULL, 16);

	r = (color_val >> 16) & 0xFF;
	g = (color_val >> 8) & 0xFF;
	b = color_val & 0xFF;

	printf("\x1b[0m");
	printf("\x1b[38;2;%d;%d;%dm", r, g, b);

	if (strcmp(hex_part, "989ef7") == 0 || strcmp(hex_part, "FF0000") == 0 || strcmp(hex_part, "7EB4DA") == 0 || strcmp(hex_part, "5277C3") == 0) {
		printf("\x1b[1m");
	}

	return 0;
}





static void print_info_line_idx(size_t idx, const struct cfetch_cfg *cfg, const char *restore_color)
{
	const struct line_cfg *lc;
	const char *fmt;
	const char *col_all;
	const char *col_lbl;
	const char *col_data;
	const char *p;

	if (idx >= cfg->lines_count)
		return;
	lc = &cfg->lines[idx];
	if (!lc->present || !lc->format)
		return;

	fmt = lc->format;
	col_all = lc->color;
	col_lbl = lc->label_color ? lc->label_color : cfg->default_label_color;
	col_data = lc->data_color ? lc->data_color : cfg->default_data_color;

	if (col_all) {
		hex_to_true_color(col_all);
		p = fmt;
		while (*p) {
			const char *b = strchr(p, '%');
			if (!b) {
				printf("%s", p);
				break;
			}
			if (b > p)
				printf("%.*s", (int)(b - p), p);
			{
				const char *e = strchr(b + 1, '%');
				if (!e) {
					printf("%s", b);
					break;
				}
				if (e > b + 1) {
					char *name = strndup(b + 1, (size_t)(e - (b + 1)));
					char *val = line_force_get_dup(lc, name);
					if (!val)
						val = placeholder_value(name);
					free(name);
					if (val) {
						printf("%s", val);
						free(val);
					}
				}
				p = e + 1;
			}
		}
		hex_to_true_color(restore_color);
		return;
	}

	p = fmt;
	while (*p) {
		const char *b = strchr(p, '%');
		if (!b) {
			if (col_lbl)
				hex_to_true_color(col_lbl);
			printf("%s", p);
			break;
		}
		if (b > p) {
			if (col_lbl)
				hex_to_true_color(col_lbl);
			printf("%.*s", (int)(b - p), p);
		}
		{
			const char *e = strchr(b + 1, '%');
			if (!e) {
				if (col_lbl)
					hex_to_true_color(col_lbl);
				printf("%s", b);
				break;
			}
			if (e > b + 1) {
				char *name = strndup(b + 1, (size_t)(e - (b + 1)));
				char *val = line_force_get_dup(lc, name);
				if (!val)
					val = placeholder_value(name);
				free(name);
				if (val) {
					if (col_data)
						hex_to_true_color(col_data);
					printf("%s", val);
					free(val);
				}
			}
			p = e + 1;
		}
	}
	hex_to_true_color(restore_color);
}

static char *line_render_plain(const struct line_cfg *lc)
{
	const char *p;
	char *out;
	size_t cap = 128;
	size_t len = 0;

	if (!lc || !lc->present || !lc->format)
		return xstrdup("");
	out = malloc(cap);
	if (!out)
		return xstrdup("");
	out[0] = '\0';
	p = lc->format;
	while (*p) {
		const char *b = strchr(p, '%');
		if (!b) {
			size_t n = strlen(p);
			if (len + n + 1 > cap) {
				size_t nc = (len + n + 1) * 2;
				char *tmp = realloc(out, nc);
				if (!tmp) {
					free(out);
					return xstrdup("");
				}
				out = tmp;
				cap = nc;
			}
			memcpy(out + len, p, n);
			len += n;
			out[len] = '\0';
			break;
		}
		if (b > p) {
			size_t n = (size_t)(b - p);
			if (len + n + 1 > cap) {
				size_t nc = (len + n + 1) * 2;
				char *tmp = realloc(out, nc);
				if (!tmp) {
					free(out);
					return xstrdup("");
				}
				out = tmp;
				cap = nc;
			}
			memcpy(out + len, p, n);
			len += n;
			out[len] = '\0';
		}
		{
			const char *e = strchr(b + 1, '%');
			if (!e) {
				size_t n = strlen(b);
				if (len + n + 1 > cap) {
					size_t nc = (len + n + 1) * 2;
					char *tmp = realloc(out, nc);
					if (!tmp) {
						free(out);
						return xstrdup("");
					}
					out = tmp;
					cap = nc;
				}
				memcpy(out + len, b, n);
				len += n;
				out[len] = '\0';
				break;
			}
			if (e > b + 1) {
				char *name = strndup(b + 1, (size_t)(e - (b + 1)));
				char *val = line_force_get_dup(lc, name);
				if (!val)
					val = placeholder_value(name);
				free(name);
				if (val) {
					size_t n = strlen(val);
					if (len + n + 1 > cap) {
						size_t nc = (len + n + 1) * 2;
						char *tmp = realloc(out, nc);
						if (!tmp) {
							free(val);
							free(out);
							return xstrdup("");
						}
						out = tmp;
						cap = nc;
					}
					memcpy(out + len, val, n);
					len += n;
					out[len] = '\0';
					free(val);
				}
			}
			p = e + 1;
		}
	}
	return out;
}

struct art_row {
	int cstart;
	int start;
	int end;
	size_t visual_len;
};

static void build_art_rows(const char *art[], struct art_row **out_rows, size_t *out_count, size_t *out_max)
{
	size_t cap = 16;
	size_t cnt = 0;
	struct art_row *rows = malloc(cap * sizeof(*rows));
	size_t max_line_length = 0;
	int i;

	if (!rows) {
		*out_rows = NULL;
		*out_count = 0;
		*out_max = 0;
		return;
	}

	for (i = 0; art[i] != NULL; ) {
		int cs = i;

		while (art[i] && art[i][0] == '#')
			i++;
		if (!art[i])
			break;

		size_t visual_len = 0;
		int j = i;

		if (art[j][0] == '$') {
			while (art[j]) {
				if (art[j][0] == '#') { j++; continue; }
				if (art[j][0] == '$') {
					visual_len += strlen(art[j] + 1);
					j++;
					continue;
				}
				break;
			}
		} else {
			visual_len += strlen(art[j]);
			j++;
			while (art[j]) {
				if (art[j][0] == '#') { j++; continue; }
				if (art[j][0] == '$') {
					visual_len += strlen(art[j] + 1);
					j++;
					continue;
				}
				break;
			}
		}

		if (cnt == cap) {
			size_t ncap = cap * 2;
			struct art_row *tmp = realloc(rows, ncap * sizeof(*rows));
			if (!tmp)
				break;
			rows = tmp;
			cap = ncap;
		}
		rows[cnt].cstart = cs;
		rows[cnt].start = i;
		rows[cnt].end = j;
		rows[cnt].visual_len = visual_len;
		if (visual_len > max_line_length)
			max_line_length = visual_len;
		cnt++;
		i = j;
	}
	*out_rows = rows;
	*out_count = cnt;
	*out_max = max_line_length;
}

static void print_art_row_segments(const char *art[], int start, int end, char *latest_hex_color, size_t latest_hex_color_sz)
{
	int k;

	for (k = start; k < end; ++k) {
		if (art[k][0] == '#') {
			hex_to_true_color(art[k]);
			strncpy(last_hexcolor, art[k], sizeof(last_hexcolor) - 1);
			last_hexcolor[sizeof(last_hexcolor) - 1] = '\0';
			strncpy(latest_hex_color, art[k], latest_hex_color_sz - 1);
			latest_hex_color[latest_hex_color_sz - 1] = '\0';
			continue;
		}
		if (art[k][0] == '$') {
			printf("%s", art[k] + 1);
		} else {
			printf("%s", art[k]);
		}
	}
}

static size_t strlen_safe(const char *s)
{
	if (!s)
		return 0;
	return strlen(s);
}

static void print_repeat_char(const char *color_hex, const char *restore_hex, char ch, size_t count)
{
	size_t i;

	if (color_hex)
		hex_to_true_color(color_hex);
	for (i = 0; i < count; i++)
		putchar(ch);
	if (restore_hex)
		hex_to_true_color(restore_hex);
}

static void print_repeat_utf8(const char *color_hex, const char *restore_hex, const char *glyph, size_t count)
{
	size_t i;

	if (color_hex)
		hex_to_true_color(color_hex);
	for (i = 0; i < count; i++)
		printf("%s", glyph);
	if (restore_hex)
		hex_to_true_color(restore_hex);
}

static void print_titled_border_line(const char *left_ch, const char *right_ch, const char *title, size_t inner_width, const char *frame_color, const char *restore_color)
{
	size_t title_len = strlen_safe(title);
	size_t total = inner_width;
	size_t left_dash;
	size_t right_dash;

	if (title_len > total)
		title_len = total;
	left_dash = (total - title_len) / 2;
	right_dash = total - title_len - left_dash;

	hex_to_true_color(frame_color);
	printf("%s", left_ch);
	print_repeat_utf8(frame_color, NULL, "─", left_dash);
	if (title && title_len > 0)
		printf("%.*s", (int)title_len, title);
	print_repeat_utf8(frame_color, NULL, "─", right_dash);
	printf("%s", right_ch);
	hex_to_true_color(restore_color);
}

static int frame_kind(const struct cfetch_cfg *cfg)
{
	char *t;
	int k = 0;

	if (!cfg->frame_type)
		return 0;
	t = str_tolower_dup(cfg->frame_type);
	if (!t)
		return 0;
	if (!strcmp(t, "none"))
		k = 0;
	else if (!strcmp(t, "underline"))
		k = 1;
	else if (!strcmp(t, "allbox"))
		k = 2;
	else if (!strcmp(t, "doublebox"))
		k = 3;
	free(t);
	return k;
}

static const char *frame_color_or_default(const struct cfetch_cfg *cfg)
{
	if (cfg->frame_color && *cfg->frame_color)
		return cfg->frame_color;
	if (cfg->default_label_color && *cfg->default_label_color)
		return cfg->default_label_color;
	return "#cccccc";
}

static void print_ascii_configured(const char *art[], const struct cfetch_cfg *cfg)
{
	char latest_hex_color[16] = "#000000";
	struct art_row *rows = NULL;
	size_t row_cnt = 0;
	size_t max_art_len = 0;
	size_t i;

	build_art_rows(art, &rows, &row_cnt, &max_art_len);

	if (frame_kind(cfg) == 0) {
		size_t line_idx = 0;
		for (i = 0; i < row_cnt; i++) {
			print_art_row_segments(art, rows[i].cstart, rows[i].end, latest_hex_color, sizeof(latest_hex_color));
			{
				int padding = (int)max_art_len - (int)rows[i].visual_len;
				int p;

				if (padding < 0)
					padding = 0;
				for (p = 0; p < padding + cfg->info_padding; ++p)
					putchar(' ');
			}
			print_info_line_idx(line_idx, cfg, latest_hex_color);
			printf("\n");
			line_idx++;
		}
		printf("\x1b[0m");
		free(rows);
		return;
	} else if (frame_kind(cfg) == 1) {
		size_t line_idx = 0;
		const char *fcol = frame_color_or_default(cfg);
		for (i = 0; i < row_cnt; i++) {
			char *plain = NULL;
			size_t plen = 0;
			print_art_row_segments(art, rows[i].cstart, rows[i].end, latest_hex_color, sizeof(latest_hex_color));
			{
				int padding = (int)max_art_len - (int)rows[i].visual_len;
				int p;

				if (padding < 0)
					padding = 0;
				for (p = 0; p < padding + cfg->info_padding; ++p)
					putchar(' ');
			}
			print_info_line_idx(line_idx, cfg, latest_hex_color);
			printf("\n");

			{
				size_t spaces = max_art_len + cfg->info_padding;
				size_t s;
				for (s = 0; s < spaces; s++)
					putchar(' ');
			}
			if (line_idx < cfg->lines_count && cfg->lines[line_idx].present && cfg->lines[line_idx].format) {
				plain = line_render_plain(&cfg->lines[line_idx]);
				plen = strlen_safe(plain);
			} else {
				plen = 0;
			}
			print_repeat_utf8(fcol, latest_hex_color, "─", plen);
			printf("\n");
			if (plain)
				free(plain);
			line_idx++;
		}
		printf("\x1b[0m");
		free(rows);
		return;
	} else if (frame_kind(cfg) == 2) {
		size_t j;
		const char *fcol = frame_color_or_default(cfg);
		size_t lines_cap = 16;
		size_t lines_cnt = 0;
		size_t *idxs = malloc(lines_cap * sizeof(*idxs));
		char **plains = NULL;
		size_t *plens = NULL;
		size_t inner_width = 0;
		size_t box_rows;
		size_t total_rows;
		size_t r;

		if (!idxs) {
			free(rows);
			return;
		}

		for (j = 0; j < cfg->lines_count; j++) {
			if (!cfg->lines[j].present || !cfg->lines[j].format)
				continue;
			if (lines_cnt == lines_cap) {
				size_t ncap = lines_cap * 2;
				size_t *t = realloc(idxs, ncap * sizeof(*idxs));
				if (!t)
					break;
				idxs = t;
				lines_cap = ncap;
			}
			idxs[lines_cnt++] = j;
		}

		plains = malloc(lines_cnt * sizeof(*plains));
		plens = malloc(lines_cnt * sizeof(*plens));
		if (!plains || !plens) {
			free(plains);
			free(plens);
			free(idxs);
			free(rows);
			return;
		}
		for (j = 0; j < lines_cnt; j++) {
			plains[j] = line_render_plain(&cfg->lines[idxs[j]]);
			plens[j] = strlen_safe(plains[j]);
			if (plens[j] > inner_width)
				inner_width = plens[j];
		}

		box_rows = 2 + lines_cnt;
		total_rows = row_cnt > box_rows ? row_cnt : box_rows;

		for (r = 0; r < total_rows; r++) {
			if (r < row_cnt) {
				print_art_row_segments(art, rows[r].cstart, rows[r].end, latest_hex_color, sizeof(latest_hex_color));
				{
					int padding = (int)max_art_len - (int)rows[r].visual_len;
					int p;

					if (padding < 0)
						padding = 0;
					for (p = 0; p < padding + cfg->info_padding; ++p)
						putchar(' ');
				}
			} else {
				int p;
				for (p = 0; p < (int)(max_art_len + cfg->info_padding); ++p)
					putchar(' ');
			}

			if (r == 0) {
				hex_to_true_color(fcol);
				printf("┌");
				print_repeat_utf8(fcol, NULL, "─", inner_width + 2);
				printf("┐");
				hex_to_true_color(latest_hex_color);
			} else if (r == box_rows - 1) {
				hex_to_true_color(fcol);
				printf("└");
				print_repeat_utf8(fcol, NULL, "─", inner_width + 2);
				printf("┘");
				hex_to_true_color(latest_hex_color);
			} else if (r - 1 < lines_cnt) {
				size_t li = r - 1;
				size_t pad = inner_width > plens[li] ? inner_width - plens[li] : 0;

				hex_to_true_color(fcol);
				printf("│ ");
				hex_to_true_color(latest_hex_color);
				print_info_line_idx(idxs[li], cfg, latest_hex_color);
				hex_to_true_color(fcol);
				{
					size_t s;
					for (s = 0; s < pad + 1; s++)
						putchar(' ');
				}
				printf("│");
				hex_to_true_color(latest_hex_color);
			}
			printf("\n");
		}

		for (j = 0; j < lines_cnt; j++)
			free(plains[j]);
		free(plains);
		free(plens);
		free(idxs);
		printf("\x1b[0m");
		free(rows);
		return;
	} else {
		size_t j;
		const char *fcol = frame_color_or_default(cfg);
		size_t soft_cap = 16, hard_cap = 16;
		size_t soft_cnt = 0, hard_cnt = 0;
		size_t *soft_idx = malloc(soft_cap * sizeof(*soft_idx));
		size_t *hard_idx = malloc(hard_cap * sizeof(*hard_idx));
		char **soft_plain = NULL, **hard_plain = NULL;
		size_t *soft_len = NULL, *hard_len = NULL;
		size_t inner_width = 0;
		size_t top_title_len = strlen_safe(cfg->frame_title_soft);
		size_t mid_title_len = strlen_safe(cfg->frame_title_hard);
		size_t rr, total_rows;
		size_t box_rows;

		if (!soft_idx || !hard_idx) {
			free(soft_idx);
			free(hard_idx);
			free(rows);
			return;
		}

		for (j = 0; j < cfg->lines_count; j++) {
			if (!cfg->lines[j].present || !cfg->lines[j].format)
				continue;
			if (cfg->lines[j].arrange_box == 1) {
				if (soft_cnt == soft_cap) {
					size_t ncap = soft_cap * 2;
					size_t *t = realloc(soft_idx, ncap * sizeof(*t));
					if (!t)
						break;
					soft_idx = t;
					soft_cap = ncap;
				}
				soft_idx[soft_cnt++] = j;
			} else if (cfg->lines[j].arrange_box == 2) {
				if (hard_cnt == hard_cap) {
					size_t ncap = hard_cap * 2;
					size_t *t = realloc(hard_idx, ncap * sizeof(*t));
					if (!t)
						break;
					hard_idx = t;
					hard_cap = ncap;
				}
				hard_idx[hard_cnt++] = j;
			}
		}

		soft_plain = malloc(soft_cnt * sizeof(*soft_plain));
		soft_len = malloc(soft_cnt * sizeof(*soft_len));
		hard_plain = malloc(hard_cnt * sizeof(*hard_plain));
		hard_len = malloc(hard_cnt * sizeof(*hard_len));
		if ((soft_cnt && (!soft_plain || !soft_len)) || (hard_cnt && (!hard_plain || !hard_len))) {
			free(soft_plain);
			free(soft_len);
			free(hard_plain);
			free(hard_len);
			free(soft_idx);
			free(hard_idx);
			free(rows);
			return;
		}

		for (j = 0; j < soft_cnt; j++) {
			soft_plain[j] = line_render_plain(&cfg->lines[soft_idx[j]]);
			soft_len[j] = strlen_safe(soft_plain[j]);
			if (soft_len[j] > inner_width)
				inner_width = soft_len[j];
		}
		for (j = 0; j < hard_cnt; j++) {
			hard_plain[j] = line_render_plain(&cfg->lines[hard_idx[j]]);
			hard_len[j] = strlen_safe(hard_plain[j]);
			if (hard_len[j] > inner_width)
				inner_width = hard_len[j];
		}
		if (top_title_len > inner_width)
			inner_width = top_title_len;
		if (mid_title_len > inner_width)
			inner_width = mid_title_len;

		box_rows = 1 + soft_cnt + 1 + hard_cnt + 1;
		total_rows = row_cnt > box_rows ? row_cnt : box_rows;

		for (rr = 0; rr < total_rows; rr++) {
			if (rr < row_cnt) {
				print_art_row_segments(art, rows[rr].cstart, rows[rr].end, latest_hex_color, sizeof(latest_hex_color));
				{
					int padding = (int)max_art_len - (int)rows[rr].visual_len;
					int p;

					if (padding < 0)
						padding = 0;
					for (p = 0; p < padding + cfg->info_padding; ++p)
						putchar(' ');
				}
			} else {
				int p;
				for (p = 0; p < (int)(max_art_len + cfg->info_padding); ++p)
					putchar(' ');
			}

			if (rr == 0) {
				print_titled_border_line("┌", "┐", cfg->frame_title_soft, inner_width + 2, fcol, latest_hex_color);
			} else if (rr > 0 && rr <= soft_cnt) {
				size_t li = rr - 1;
				size_t pad = inner_width > soft_len[li] ? inner_width - soft_len[li] : 0;

				hex_to_true_color(fcol);
				printf("│ ");
				hex_to_true_color(latest_hex_color);
				print_info_line_idx(soft_idx[li], cfg, latest_hex_color);
				hex_to_true_color(fcol);
				{
					size_t s;
					for (s = 0; s < pad + 1; s++)
						putchar(' ');
				}
				printf("│");
				hex_to_true_color(latest_hex_color);
			} else if (rr == soft_cnt + 1) {
				print_titled_border_line("├", "┤", cfg->frame_title_hard, inner_width + 2, fcol, latest_hex_color);
			} else if (rr > soft_cnt + 1 && rr <= soft_cnt + 1 + hard_cnt) {
				size_t li = rr - (soft_cnt + 2);
				size_t pad = inner_width > hard_len[li] ? inner_width - hard_len[li] : 0;

				hex_to_true_color(fcol);
				printf("│ ");
				hex_to_true_color(latest_hex_color);
				print_info_line_idx(hard_idx[li], cfg, latest_hex_color);
				hex_to_true_color(fcol);
				{
					size_t s;
					for (s = 0; s < pad + 1; s++)
						putchar(' ');
				}
				printf("│");
				hex_to_true_color(latest_hex_color);
			} else if (rr == box_rows - 1) {
				hex_to_true_color(fcol);
				printf("└");
				print_repeat_utf8(fcol, NULL, "─", inner_width + 2);
				printf("┘");
				hex_to_true_color(latest_hex_color);
			}
			printf("\n");
		}

		for (j = 0; j < soft_cnt; j++)
			free(soft_plain[j]);
		for (j = 0; j < hard_cnt; j++)
			free(hard_plain[j]);
		free(soft_plain);
		free(soft_len);
		free(hard_plain);
		free(hard_len);
		free(soft_idx);
		free(hard_idx);
		printf("\x1b[0m");
		free(rows);
		return;
	}
}

static const char **auto_art_by_distro(char *distro)
{
	char *distro_lower;
	const char **ret;

	if (distro == NULL)
		return (const char **)tux_ascii;
	distro_lower = str_tolower_dup(distro);
	if (!distro_lower)
		return (const char **)tux_ascii;

	if (strcmp(distro_lower, "arch") == 0 || strcmp(distro_lower, "archlinux") == 0) {
		ret = (const char **)arch_ascii_classic;
	} else if (strcmp(distro_lower, "fedora") == 0) {
		ret = (const char **)fedora_ascii;
	} else if (strcmp(distro_lower, "gentoo") == 0) {
		ret = (const char **)gentoo_ascii;
	} else if (strcmp(distro_lower, "redhat") == 0 || strcmp(distro_lower, "rhel") == 0) {
		ret = (const char **)redhat_ascii;
	} else if (strcmp(distro_lower, "linuxmint") == 0 || strcmp(distro_lower, "mint") == 0) {
		ret = (const char **)mint_ascii;
	} else if (strcmp(distro_lower, "slackware") == 0) {
		ret = (const char **)slackware_ascii;
	} else if (strcmp(distro_lower, "debian") == 0) {
		ret = (const char **)debian_ascii;
	} else {
		ret = (const char **)tux_ascii;
	}
	free(distro_lower);
	return ret;
}

static const char **select_art_by_name(const char *name, char *distro)
{
	char *l;
	const char **ret;

	if (!name || !*name)
		return (const char **)tux_ascii;
	l = str_tolower_dup(name);
	if (!l)
		return (const char **)tux_ascii;

	if (strcmp(l, "auto") == 0) {
		free(l);
		return auto_art_by_distro(distro);
	}
	if (strcmp(l, "arch") == 0) ret = (const char **)arch_ascii;
	else if (strcmp(l, "arch-classic") == 0) ret = (const char **)arch_ascii_classic;
	else if (strcmp(l, "arch-alt") == 0) ret = (const char **)arch_ascii_alt;
	else if (strcmp(l, "fedora") == 0) ret = (const char **)fedora_ascii;
	else if (strcmp(l, "gentoo") == 0) ret = (const char **)gentoo_ascii;
	else if (strcmp(l, "redhat") == 0) ret = (const char **)redhat_ascii;
	else if (strcmp(l, "rhel") == 0) ret = (const char **)redhat_ascii;
	else if (strcmp(l, "mint") == 0) ret = (const char **)mint_ascii;
	else if (strcmp(l, "linuxmint") == 0) ret = (const char **)mint_ascii;
	else if (strcmp(l, "slackware") == 0) ret = (const char **)slackware_ascii;
	else if (strcmp(l, "debian") == 0) ret = (const char **)debian_ascii;
	else if (strcmp(l, "tux") == 0) ret = (const char **)tux_ascii;
	else if (strcmp(l, "apple") == 0) ret = (const char **)apple_ascii;
	else if (strcmp(l, "apple-mini") == 0) ret = (const char **)apple_ascii_mini;
	else if (strcmp(l, "custom") == 0) ret = (const char **)custom_ascii;
	else if (strcmp(l, "custom2") == 0) ret = (const char **)custom2_ascii;
	else if (strcmp(l, "dota") == 0) ret = (const char **)dota_ascii;
	else if (strcmp(l, "nixos") == 0) ret = (const char **)nixos_ascii;
	else ret = (const char **)tux_ascii;

	free(l);
	return ret;
}

int main(int argc, char *argv[])
{
	struct cfetch_cfg cfg;
	char *distro;
	const char **art = NULL;

	cfg_init_defaults(&cfg);
	cfg_load(&cfg);

	distro = get_distro();

	if (argc >= 3 && strcmp(argv[1], "--ExportAscii") == 0) {
		const char *filename = argv[2];
		printf("// Generated by cfetch --ExportAscii %s\n", filename);
		printf("// Insert this code into your C:\n\n");
		{
			int rc = export_ascii_art(filename);
			if (distro)
				free(distro);
			cfg_free(&cfg);
			return rc;
		}
	}

	if (argc == 1) {
		art = select_art_by_name(cfg.ascii_art, distro);
		print_ascii_configured(art, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--arch") == 0) {
		print_ascii_configured((const char **)arch_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--arch-classic") == 0) {
		print_ascii_configured((const char **)arch_ascii_classic, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--redhat") == 0) {
		print_ascii_configured((const char **)redhat_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--apple-mini") == 0) {
		print_ascii_configured((const char **)apple_ascii_mini, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--custom") == 0) {
		print_ascii_configured((const char **)custom_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--fedora") == 0) {
		print_ascii_configured((const char **)fedora_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--gentoo") == 0) {
		print_ascii_configured((const char **)gentoo_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--tux") == 0) {
		print_ascii_configured((const char **)tux_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--apple") == 0) {
		print_ascii_configured((const char **)apple_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--mint") == 0) {
		print_ascii_configured((const char **)mint_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--slackware") == 0) {
		print_ascii_configured((const char **)slackware_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--debian") == 0) {
		print_ascii_configured((const char **)debian_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--arch-alt") == 0) {
		print_ascii_configured((const char **)arch_ascii_alt, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--dota") == 0) {
		print_ascii_configured((const char **)dota_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--nixos") == 0) {
		print_ascii_configured((const char **)nixos_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--endeavour") == 0) {
		print_ascii_configured((const char **)endeavour_ascii, &cfg);
	} else if (argc == 2 && strcmp(argv[1], "--void") == 0) {
		print_ascii_configured((const char **)void_ascii, &cfg);
	}
	else {
		fprintf(stderr, "Error: Invalid arguments.\n");
		fprintf(stderr, "Usage: %s [no args -> config ascii_art] or flags: --arch | --redhat | --apple-mini | --fedora | --gentoo | --tux | --apple | --mint | --slackware | --debian | --arch-alt | --dota | --nixos | --ExportAscii <filename>\n", argv[0]);
		cfg_free(&cfg);
		if (distro) free(distro);
		return 1;
	}

	if (distro != NULL)
		free(distro);
	cfg_free(&cfg);
	return 0;
}