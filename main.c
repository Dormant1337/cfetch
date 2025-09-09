#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ascii.c"
#define _POSIX_C_SOURCE 200809L
#include <glob.h>
#include <ctype.h>
#include <unistd.h>
#include <unistd.h>
#include <pwd.h>

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif
#include <fcntl.h>
#include <bits/waitflags.h>

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define MAX_ASCII_FILE_LINES            1000
#define MAX_ASCII_FILE_LINE_LENGTH      512

int spaces_before_blocks = 3;
char characteristics_sentence_2[128];
char characteristics_sentence_1[128];
char last_hexcolor[16];
char *get_shell_info(void);

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

	FILE *f = fopen(vendor_file, "r");
	if (!f) return NULL;
	if (!fgets(buffer, sizeof(buffer), f)) { fclose(f); return NULL; }
	fclose(f);
	buffer[strcspn(buffer, "\n")] = 0;

	char vendor[64];
	sscanf(buffer, "%63s", vendor);

	f = fopen(name_file, "r");
	if (!f) return NULL;
	if (!fgets(buffer, sizeof(buffer), f)) { fclose(f); return NULL; }
	fclose(f);
	buffer[strcspn(buffer, "\n")] = 0;

	char *model = buffer;
	while (*model && isspace((unsigned char)*model)) model++;
	char *end = model + strlen(model) - 1;
	while (end > model && isspace((unsigned char)*end)) *end-- = '\0';

	char *result = malloc(strlen(vendor) + 1 + strlen(model) + 1);
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
	if (!wm_raw)
		return NULL;


	const char *colon = strchr(wm_raw, ':');
	size_t len = colon ? (size_t)(colon - wm_raw) : strlen(wm_raw);

	char *wm = malloc(len + 1);
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

char* get_cpu_name(void) {
	FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
	if (cpuinfo == NULL) {
		perror("fopen");
		return NULL;
	}

	char line[256];
	char* cpu_name = NULL;

	while (fgets(line, sizeof(line), cpuinfo) != NULL) {
		if (strncmp(line, "model name", 10) == 0) {
			char* colon = strchr(line, ':');
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
	if (!escaped) {
		perror("Memory allocation error for escaped string");
		return NULL;
	}

	char *writer = escaped;
	for (size_t i = 0; i < len; ++i) {
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
	FILE                   *fp;
	char                  **art_lines;
	char                    buffer[MAX_ASCII_FILE_LINE_LENGTH];
	int                     line_count;
	int                     i;

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

	if (!hex_color)
		return -1;

	if (*ptr == '#')
		ptr++;
	
	size_t len_ptr = strlen(ptr);
	if (len_ptr != 6 && len_ptr != 8) {
		return -1;
	}

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


void characteristics_show(int processing_line)
{
	characteristics_sentence_2[0] = '\0';
	characteristics_sentence_1[0] = '\0';

	if (processing_line == 2) {
		char *host = get_motherboard();
		if (host != NULL) {
			snprintf(characteristics_sentence_2, sizeof(characteristics_sentence_2), "%s", host);
			snprintf(characteristics_sentence_1, sizeof(characteristics_sentence_1), "Host:");
			free(host);
		}
	} else if (processing_line == 3) {
		char *sh = get_shell_info();
		if (sh != NULL) {
			snprintf(characteristics_sentence_2, sizeof(characteristics_sentence_2), "%s", sh);
			snprintf(characteristics_sentence_1, sizeof(characteristics_sentence_1), "Shell:");
			free(sh);
		}
	} else if (processing_line == 4) {
		char *cpu = get_cpu_name();
		if (cpu != NULL) {
			snprintf(characteristics_sentence_2, sizeof(characteristics_sentence_2), "%s", cpu);
			snprintf(characteristics_sentence_1, sizeof(characteristics_sentence_1), "CPU:");
			free(cpu);
		}
	} else if (processing_line == 5) {
		char *gpu = get_gpu_name();
		if (gpu != NULL) {
			snprintf(characteristics_sentence_2, sizeof(characteristics_sentence_2), "%s", gpu);
			snprintf(characteristics_sentence_1, sizeof(characteristics_sentence_1), "GPU:");
			free(gpu);
		}
	} else if (processing_line == 6) {
		char *memory = get_memory();
		if (memory != NULL) {
			snprintf(characteristics_sentence_2, sizeof(characteristics_sentence_2), "%s", memory);
			snprintf(characteristics_sentence_1, sizeof(characteristics_sentence_1), "RAM:");
			free(memory);
		}
	} else if (processing_line == 7) {
		char *distro = get_distro();
		if (distro != NULL) {
			capitalize_first(distro);
			snprintf(characteristics_sentence_2, sizeof(characteristics_sentence_2), "%s", distro);
			snprintf(characteristics_sentence_1, sizeof(characteristics_sentence_1), "OS:");
			free(distro);
		}
	} else if (processing_line == 8) {
		char *wm = get_wm_clean();
		if (wm != NULL) {
			capitalize_first(wm);
			snprintf(characteristics_sentence_2, sizeof(characteristics_sentence_2), "%s", wm);
			snprintf(characteristics_sentence_1, sizeof(characteristics_sentence_1), "WM:");
			free(wm);
		}
	}
}

void print_color_blocks_raw(void) {
	const char *colors[] = {
		"#FF0000", "#00FF00", "#0000FF", "#FFFF00",
		"#FF00FF", "#00FFFF", "#FFFFFF", "#808080"
	};
	int num_colors = sizeof(colors) / sizeof(colors[0]);
	for (int i = 0; i < num_colors; ++i) {
		long color_val;
		int r, g, b;
		const char *ptr = colors[i];
		char hex_part[7];

		if (*ptr == '#')
			ptr++;
		strncpy(hex_part, ptr, 6);
		hex_part[6] = '\0';
		color_val = strtol(hex_part, NULL, 16);

		r = (color_val >> 16) & 0xFF;
		g = (color_val >> 8) & 0xFF;
		b = color_val & 0xFF;


		printf("\x1b[48;2;%d;%d;%dm  ", r, g, b);
	}
	printf("\x1b[0m");
}


void print_ascii(const char *art[])
{
	char latest_hex_color[16] = "#000000";
	char white_color[16] = "#ffffffff";
	int visual_line_number = 0;
	size_t max_line_length = 0;

	for (int i = 0; art[i] != NULL; ) {
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

	for (int i = 0; art[i] != NULL; ) {
		while (art[i] && art[i][0] == '#') {
			hex_to_true_color(art[i]);
			strncpy(last_hexcolor, art[i], sizeof(last_hexcolor) - 1);
			last_hexcolor[sizeof(last_hexcolor) - 1] = '\0';
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
				continue;
			}
			if (art[k][0] == '$') {
				printf("%s", art[k] + 1);
			} else {
				printf("%s", art[k]);
			}
		}

		int padding = (int)max_line_length - (int)visual_len;
		if (padding < 0) padding = 0;
		for (int p = 0; p < padding + 3; ++p)
			putchar(' ');

		visual_line_number++;
		characteristics_show(visual_line_number);

		bool did_print_info = false;
		if (characteristics_sentence_1[0] != '\0') {
			hex_to_true_color(white_color);
			printf("%s %s", characteristics_sentence_1, characteristics_sentence_2);
			hex_to_true_color(last_hexcolor);
			did_print_info = true;
		}

		if (visual_line_number == 10) {
			if (did_print_info) {
				printf("   "); 
			}
			for (int i = 0; i < spaces_before_blocks; i++) {
				printf(" ");
			}
			print_color_blocks_raw(); 
			hex_to_true_color(last_hexcolor);
		}
		
		printf("\n");
		i = j;
	}

	printf("\x1b[0m");
}


void print_ascii_by_distro(char *distro) {
	if (distro == NULL) {
		print_ascii(tux_ascii);
		return;
	}
	char *distro_lower = strdup(distro);
	if (distro_lower == NULL) {
		print_ascii(tux_ascii);
		return;
	}
	for (char *p = distro_lower; *p; ++p) *p = tolower((unsigned char)*p);

	if (strcmp(distro_lower, "arch") == 0 || strcmp(distro_lower, "archlinux") == 0) {
		print_ascii(arch_ascii_classic);
	} else if (strcmp(distro_lower, "fedora") == 0) {
		print_ascii(fedora_ascii);
	} else if (strcmp(distro_lower, "gentoo") == 0) {
		print_ascii(gentoo_ascii);
	} else if (strcmp(distro_lower, "redhat") == 0 || strcmp(distro_lower, "rhel") == 0) {
		print_ascii(redhat_ascii);
	} else if (strcmp(distro_lower, "linuxmint") == 0 || strcmp(distro_lower, "mint") == 0) {
		print_ascii(mint_ascii);
	} else if (strcmp(distro_lower, "slackware") == 0) {
		print_ascii(slackware_ascii);
	} else if (strcmp(distro_lower, "debian") == 0) {
		print_ascii(debian_ascii);
	}
	else {
		print_ascii(tux_ascii);
	}
	free(distro_lower);
}


int main(int argc, char *argv[])
{
	char *distro = get_distro();

	printf("%s\n\n", distro);

	if (argc == 1) {
		print_ascii_by_distro(distro);
	} else if (argc == 2 && strcmp(argv[1], "--arch") == 0) {
		print_ascii(arch_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--arch-classic") == 0) {
		print_ascii(arch_ascii_classic); 
	} else if (argc == 2 && strcmp(argv[1], "--redhat") == 0) {
		print_ascii(redhat_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--apple-mini") == 0) {
		print_ascii(apple_ascii_mini);
	} else if (argc == 2 && strcmp(argv[1], "--custom") == 0) {
		print_ascii(custom_ascii); 
	} else if (argc == 2 && strcmp(argv[1], "--fedora") == 0) {
		print_ascii(fedora_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--gentoo") == 0) {
		print_ascii(gentoo_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--tux") == 0) {
		print_ascii(tux_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--apple") == 0) {
		print_ascii(apple_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--mint") == 0) {
		print_ascii(mint_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--slackware") == 0) {
		print_ascii(slackware_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--debian") == 0) {
		print_ascii(debian_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--arch-alt") == 0) {
		print_ascii(arch_ascii_alt);
	} else if (argc == 2 && strcmp(argv[1], "--dota") == 0) {
		print_ascii(dota_ascii);
	} else if (argc == 2 && strcmp(argv[1], "--nixos") == 0) {
		print_ascii(nixos_ascii);
	}
	else if (argc >= 3 && strcmp(argv[1], "--ExportAscii") == 0) {
		const char *filename = argv[2];
		printf("// Generated by cfetch --ExportAscii %s\n", filename);
		printf("// Insert this code into your C:\n\n");
		return export_ascii_art(filename);
	}
	else {
		fprintf(stderr, "Error: Invalid arguments.\n");
		fprintf(stderr, "Usage: %s [--arch | --redhat | --apple-mini | --fedora | --gentoo | --tux | --apple | --mint | --slackware | --debian | --ExportAscii <filename>]\n", argv[0]);
		return 1;
	}

	if (distro != NULL) {
		free(distro);
	}

	return 0;
}