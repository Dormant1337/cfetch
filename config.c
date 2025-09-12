#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* Files includes. */
#include "config.h"
#include "utils.h"

#define MAX_ASCII_FILE_LINES            1000
#define MAX_ASCII_FILE_LINE_LENGTH      512

void cfg_init_defaults(struct cfetch_cfg *cfg)
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

void cfg_ensure_line(struct cfetch_cfg *cfg, size_t idx)
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

void cfg_free(struct cfetch_cfg *cfg)
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

void line_force_set(struct line_cfg *lc, const char *key, const char *val)
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

char *line_force_get_dup(const struct line_cfg *lc, const char *key)
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

char *config_path(void)
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

int cfg_load(struct cfetch_cfg *cfg)
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
