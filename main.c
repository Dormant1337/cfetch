#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ascii.c"
#define _POSIX_C_SOURCE 200809L
#include <glob.h>
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <time.h>

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
};

struct cfetch_cfg {
	char *ascii_art;
	char *default_label_color;
	char *default_data_color;
	int info_padding;
	struct line_cfg *lines;
	size_t lines_count;
};

static char *xstrdup(const char *s)
{
	size_t n;
	char *p;

	if (!s)
		return NULL;
	n = strlen(s) + 1;
	p = malloc(n);
	if (!p)
		return NULL;
	memcpy(p, s, n);
	return p;
}

static char *str_tolower_dup(const char *s)
{
	size_t n, i;
	char *p;

	if (!s)
		return NULL;
	n = strlen(s);
	p = malloc(n + 1);
	if (!p)
		return NULL;
	for (i = 0; i < n; i++)
		p[i] = (char)tolower((unsigned char)s[i]);
	p[n] = '\0';
	return p;
}

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
			char *val = NULL;
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
				val = xstrdup(p + 1);
			} else {
				rtrim_inplace(p);
				val = xstrdup(p);
			}
			if (name && val)
				line_force_set(lc, name, val);
done_force:
			free(name);
			free(val);
		}
			continue;
		}
	}
	free(line);
	fclose(f);
	return 1;
}

char *get_shell_info(void)
{
	char			*env;
	char			shell_path[256];
	char			shell_name[128];
	char			cmd[512];
	FILE			*pipe;
	char			line[512];
	char			*res;
	const char		*args[] = { "--version", "-version", "-v", "-V", NULL };
	int			i;
	char			*slash;

	env = getenv("SHELL");
	if (env && *env)
		snprintf(shell_path, sizeof(shell_path), "%s", env);
	else {
		struct passwd	*pw = getpwuid(getuid());

		if (pw && pw->pw_shell && *pw->pw_shell)
			snprintf(shell_path, sizeof(shell_path), "%s", pw->pw_shell);
		else
			return strdup("unknown");
	}

	slash = strrchr(shell_path, '/');
	if (slash)
		snprintf(shell_name, sizeof(shell_name), "%s", slash + 1);
	else
		snprintf(shell_name, sizeof(shell_name), "%s", shell_path);

	for (i = 0; args[i]; i++) {
		snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null", shell_path, args[i]);
		pipe = popen(cmd, "r");
		if (!pipe)
			continue;
		if (fgets(line, sizeof(line), pipe) != NULL) {
			char		*p = line;

			line[strcspn(line, "\r\n")] = 0;
			while (*p == ' ' || *p == '\t')
				p++;
			if (!strncmp(p, shell_name, strlen(shell_name))) {
				res = strdup(p);
			} else {
				res = malloc(strlen(shell_name) + 1 + strlen(p) + 1);
				if (res)
					sprintf(res, "%s %s", shell_name, p);
			}
			pclose(pipe);
			return res ? res : strdup(shell_name);
		}
		pclose(pipe);
	}

	{
		const char	*ver = NULL;

		if (!strcmp(shell_name, "bash"))
			ver = getenv("BASH_VERSION");
		else if (!strcmp(shell_name, "zsh"))
			ver = getenv("ZSH_VERSION");
		else if (!strcmp(shell_name, "fish"))
			ver = getenv("FISH_VERSION");
		else if (!strcmp(shell_name, "ksh"))
			ver = getenv("KSH_VERSION");

		if (ver && *ver) {
			res = malloc(strlen(shell_name) + 1 + strlen(ver) + 1);
			if (res) {
				sprintf(res, "%s %s", shell_name, ver);
				return res;
			}
		}
	}

	return strdup(shell_name);
}

char *get_memory(void)
{
	const char *Gigabyte = "GB";
	FILE *f = fopen("/proc/meminfo", "r");
	if (!f) return NULL;

	char line[256];
	unsigned long mem_total_kb = 0;
	unsigned long mem_available_kb = 0;

	while (fgets(line, sizeof(line), f)) {
		if (sscanf(line, "MemTotal: %lu kB", &mem_total_kb) == 1) continue;
		if (sscanf(line, "MemAvailable: %lu kB", &mem_available_kb) == 1) continue;
		if (mem_total_kb && mem_available_kb) break;
	}
	fclose(f);

	if (mem_total_kb == 0) return NULL;

	double total_gb = mem_total_kb / 1024.0 / 1024.0;
	double used_gb = (mem_total_kb - mem_available_kb) / 1024.0 / 1024.0;

	char *result = malloc(32);
	if (!result) return NULL;
	snprintf(result, 32, "%.1f/%.1f %s", used_gb, total_gb, Gigabyte);
	return result;
}

char *get_motherboard(void)
{
	const char *vendor_file = "/sys/class/dmi/id/board_vendor";
	const char *name_file   = "/sys/class/dmi/id/board_name";

	char buffer[256];
	char vendor[64];
	FILE *f;
	char *model;
	size_t len;
	char *result;
	char *end;

	f = fopen(vendor_file, "r");
	if (!f) return NULL;
	if (!fgets(buffer, sizeof(buffer), f)) { fclose(f); return NULL; }
	fclose(f);
	buffer[strcspn(buffer, "\n")] = 0;
	sscanf(buffer, "%63s", vendor);

	f = fopen(name_file, "r");
	if (!f) return NULL;
	if (!fgets(buffer, sizeof(buffer), f)) { fclose(f); return NULL; }
	fclose(f);
	buffer[strcspn(buffer, "\n")] = 0;

	model = buffer;
	while (*model && isspace((unsigned char)*model)) model++;
	len = strlen(model);
	if (len == 0)
		return xstrdup(vendor);
	end = model + len - 1;
	while (end > model && isspace((unsigned char)*end)) *end-- = '\0';

	result = malloc(strlen(vendor) + 1 + strlen(model) + 1);
	if (!result) return NULL;
	sprintf(result, "%s %s", vendor, model);
	return result;
}

void capitalize_first(char *s)
{
	if (s && s[0] != '\0')
		s[0] = toupper((unsigned char)s[0]);
}

char *get_wm(void)
{
	char *env;

	env = getenv("XDG_CURRENT_DESKTOP");
	if (env && *env)
		return env;

	env = getenv("DESKTOP_SESSION");
	if (env && *env)
		return env;

	env = getenv("GDMSESSION");
	if (env && *env)
		return env;

	return "unknown";
}

char *get_wm_clean(void)
{
	const char *wm_raw = get_wm();
	const char *colon;
	size_t len;
	char *wm;

	if (!wm_raw)
		return NULL;
	colon = strchr(wm_raw, ':');
	len = colon ? (size_t)(colon - wm_raw) : strlen(wm_raw);
	wm = malloc(len + 1);
	if (!wm)
		return NULL;
	strncpy(wm, wm_raw, len);
	wm[len] = '\0';
	return wm;
}

char *get_distro(void)
{
	FILE *f = NULL;
	glob_t gl;
	char *line = NULL;
	size_t len = 0;
	ssize_t nread;
	char *result = NULL;

	if (glob("/etc/*-release", 0, NULL, &gl) != 0)
		return NULL;

	for (size_t i = 0; i < gl.gl_pathc && result == NULL; ++i) {
		f = fopen(gl.gl_pathv[i], "r");
		if (!f)
			continue;

		while ((nread = getline(&line, &len, f)) != -1) {
			char *s = line;
			while (*s == ' ' || *s == '\t')
				++s;
			if (*s == '\0' || *s == '\n' || *s == '#')
				continue;
			if (strncmp(s, "ID=", 3) != 0)
				continue;
			char *val = s + 3;
			char *eol = strchr(val, '\n');
			if (eol) *eol = '\0';
			while (*val == ' ' || *val == '\t')
				++val;
			if (*val == '"' || *val == '\'') {
				char quote = *val++;
				char *endq = strrchr(val, quote);
				if (endq)
					*endq = '\0';
			} else {
				char *t = val + strlen(val) - 1;
				while (t >= val && (*t == ' ' || *t == '\t')) {
					*t = '\0';
					--t;
				}
			}
			result = strdup(val);
			if (result == NULL) {
				free(line);
				fclose(f);
				globfree(&gl);
				return NULL;
			}
			break;
		}

		free(line);
		line = NULL;
		len = 0;
		fclose(f);
		f = NULL;
	}

	globfree(&gl);
	return result;
}

char* get_gpu_name(void)
{
	char buffer[256];
	char* gpu_name = NULL;
	FILE* pipe = NULL;

#if defined(__linux__) || defined(__APPLE__)
	#if defined(__linux__)
	pipe = popen("lspci | grep -i 'VGA\\|3D\\|Display'", "r");
	#elif defined(__APPLE__)
	pipe = popen("system_profiler SPDisplaysDataType | grep 'Chipset Model'", "r");
	#endif
	
	if (pipe == NULL) {
		perror("popen");
		return NULL;
	}

	if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
		char* start_bracket = strchr(buffer, '[');
		char* end_bracket = strchr(buffer, ']');

		if (start_bracket != NULL && end_bracket != NULL && start_bracket < end_bracket) {
			char* name_start = start_bracket + 1;
			size_t name_len = end_bracket - name_start;
			
			gpu_name = (char*)malloc(name_len + 1);
			if (gpu_name != NULL) {
				strncpy(gpu_name, name_start, name_len);
				gpu_name[name_len] = '\0';
			}
		} else {
			char* name_start = strchr(buffer, ':');
			if (name_start != NULL) {
				name_start += 2;
				while (*name_start == ' ') {
					name_start++;
				}
				name_start[strcspn(name_start, "\n")] = '\0';
				gpu_name = strdup(name_start);
			}
		}
	}
	pclose(pipe);

#elif defined(_WIN32)
	pipe = _popen("wmic path win32_videocontroller get name", "r");
	if (pipe == NULL) {
		perror("_popen");
		return NULL;
	}

	fgets(buffer, sizeof(buffer), pipe);

	if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
		int len = strlen(buffer);
		while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r' || buffer[len - 1] == ' ')) {
			buffer[--len] = '\0';
		}
		if (strlen(buffer) > 0) {
			gpu_name = strdup(buffer);
		}
	}
	_pclose(pipe);
#else
	gpu_name = strdup("GPU not supported on this OS");
#endif

	return gpu_name;
}

char* get_cpu_name(void)
{
	FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
	char line[256];
	char* cpu_name = NULL;
	char* colon;

	if (cpuinfo == NULL)
		return NULL;

	while (fgets(line, sizeof(line), cpuinfo) != NULL) {
		if (strncmp(line, "model name", 10) == 0) {
			colon = strchr(line, ':');
			if (colon != NULL) {
				char* name_start = colon + 2;
				name_start[strcspn(name_start, "\n")] = '\0';
				cpu_name = strdup(name_start);
			}
			break;
		}
	}

	fclose(cpuinfo);
	return cpu_name;
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

static char *get_username_str(void)
{
	const char *u;
	struct passwd *pw;

	u = getenv("USER");
	if (u && *u)
		return xstrdup(u);
	pw = getpwuid(getuid());
	if (pw && pw->pw_name)
		return xstrdup(pw->pw_name);
	return xstrdup("unknown");
}

static char *get_hostname_str(void)
{
	char buf[256];

	if (gethostname(buf, sizeof(buf)) != 0)
		return xstrdup("unknown");
	buf[sizeof(buf) - 1] = '\0';
	return xstrdup(buf);
}

static char *get_kernel_str(void)
{
	struct utsname u;

	if (uname(&u) != 0)
		return xstrdup("unknown");
	return xstrdup(u.release);
}

static char *get_shell_str(void)
{
	const char *s = getenv("SHELL");
	if (s && *s)
		return xstrdup(s);
	return xstrdup("unknown");
}

static char *get_uptime_str(void)
{
#if defined(__linux__)
	FILE *f;
	double up;
	unsigned long sec;
	unsigned long d, h, m;
	char *res;

	f = fopen("/proc/uptime", "r");
	if (!f)
		return xstrdup("unknown");
	if (fscanf(f, "%lf", &up) != 1) {
		fclose(f);
		return xstrdup("unknown");
	}
	fclose(f);
	sec = (unsigned long)up;
	d = sec / 86400;
	sec %= 86400;
	h = sec / 3600;
	sec %= 3600;
	m = sec / 60;
	res = malloc(64);
	if (!res)
		return NULL;
	if (d > 0)
		snprintf(res, 64, "%lud %luh %lum", d, h, m);
	else if (h > 0)
		snprintf(res, 64, "%luh %lum", h, m);
	else
		snprintf(res, 64, "%lum", m);
	return res;
#else
	return xstrdup("unknown");
#endif
}

static char *get_os_str(void)
{
	char *d = get_distro();
	if (!d)
		return xstrdup("unknown");
	capitalize_first(d);
	return d;
}

static char *get_host_str(void)
{
	return get_motherboard();
}

static char *get_cpu_str(void)
{
	return get_cpu_name();
}

static char *get_gpu_str(void)
{
	return get_gpu_name();
}

static char *get_ram_str(void)
{
	return get_memory();
}

static char *get_wm_str(void)
{
	char *w = get_wm_clean();
	if (!w)
		return xstrdup("unknown");
	capitalize_first(w);
	return w;
}

static char *placeholder_value(const char *name)
{
	char *n;

	n = str_tolower_dup(name);
	if (!n)
		return xstrdup("");
	if (strcmp(n, "username") == 0) {
		free(n);
		return get_username_str();
	}
	if (strcmp(n, "hostname") == 0) {
		free(n);
		return get_hostname_str();
	}
	if (strcmp(n, "os") == 0) {
		free(n);
		return get_os_str();
	}
	if (strcmp(n, "host") == 0) {
		free(n);
		return get_host_str();
	}
	if (strcmp(n, "kernel") == 0) {
		free(n);
		return get_kernel_str();
	}
	if (strcmp(n, "cpu") == 0) {
		free(n);
		return get_cpu_str();
	}
	if (strcmp(n, "gpu") == 0) {
		free(n);
		return get_gpu_str();
	}
	if (strcmp(n, "ram") == 0) {
		free(n);
		return get_ram_str();
	}
	if (strcmp(n, "shell") == 0) {
		free(n);
		return get_shell_str();
	}
	if (strcmp(n, "uptime") == 0) {
		free(n);
		return get_uptime_str();
	}
	if (strcmp(n, "wm") == 0) {
		free(n);
		return get_wm_str();
	}
	if (strcmp(n, "shell_info") == 0) {
		free(n);
		return get_shell_info();
	}	
	free(n);
	return xstrdup("");
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

static void compute_max_visual_length(const char *art[], size_t *out_max)
{
	size_t max_line_length = 0;
	int i;

	for (i = 0; art[i] != NULL; ) {
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

		if (visual_len > max_line_length)
			max_line_length = visual_len;

		i = j;
	}
	*out_max = max_line_length;
}

static void print_ascii_configured(const char *art[], const struct cfetch_cfg *cfg)
{
	char latest_hex_color[16] = "#000000";
	size_t max_line_length = 0;
	size_t line_idx = 0;
	int i;

	compute_max_visual_length(art, &max_line_length);

	for (i = 0; art[i] != NULL; ) {
		while (art[i] && art[i][0] == '#') {
			hex_to_true_color(art[i]);
			strncpy(last_hexcolor, art[i], sizeof(last_hexcolor) - 1);
			last_hexcolor[sizeof(last_hexcolor) - 1] = '\0';
			strncpy(latest_hex_color, art[i], sizeof(latest_hex_color) - 1);
			latest_hex_color[sizeof(latest_hex_color) - 1] = '\0';
			i++;
		}
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

		for (int k = i; k < j; ++k) {
			if (art[k][0] == '#') {
				hex_to_true_color(art[k]);
				strncpy(last_hexcolor, art[k], sizeof(last_hexcolor) - 1);
				last_hexcolor[sizeof(last_hexcolor) - 1] = '\0';
				strncpy(latest_hex_color, art[k], sizeof(latest_hex_color) - 1);
				latest_hex_color[sizeof(latest_hex_color) - 1] = '\0';
				continue;
			}
			if (art[k][0] == '$') {
				printf("%s", art[k] + 1);
			} else {
				printf("%s", art[k]);
			}
		}

		{
			int padding = (int)max_line_length - (int)visual_len;
			int p;

			if (padding < 0) padding = 0;
			for (p = 0; p < padding + cfg->info_padding; ++p)
				putchar(' ');
		}

		print_info_line_idx(line_idx, cfg, latest_hex_color);
		printf("\n");
		i = j;
		line_idx++;
	}

	printf("\x1b[0m");
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
		return export_ascii_art(filename);
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
	} else {
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