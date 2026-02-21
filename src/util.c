#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <unistd.h>

char **info_list = NULL;
int info_count = 0;


#include "ascii.h"
#include "util.h"
#include "fetch/hw.h"
#include "fetch/sw.h"

int get_term_width() {
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		return 80;
	}
	return w.ws_col;
}

int check_end_newline(const char *str) {
	size_t len = strlen(str);
	if (len == 0) return 0;

	if (str[len - 1] == '\n') return 1;

	if (len >= 2 && str[len - 2] == '\\' && str[len - 1] == 'n') return 1;

	return 0;
}

int utf8_strlen(const char *s, bool ignore_spaces) {
	int count = 0;
	while (*s) {
		if ((*s & 0xC0) != 0x80) {
			if (!(ignore_spaces && *s == ' ')) {
				count++;
			}
		}
		s++;
	}
	return count;
}

void hex_printf(const char *hex, const char *format, ...) {
	unsigned int r, g, b;

	if (hex[0] == '#') {
                hex++;
	}

	if (sscanf(hex, "%02x%02x%02x", &r, &g, &b) != 3) {
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		return;
	}

	printf("\033[38;2;%u;%u;%um", r, g, b);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);

	printf("\033[0m");
}

void copy_without_last_two(char *dest, const char *src) {
	size_t len = strlen(src);
	int to_cut = 0;

	if (len > 0 && src[len - 1] == '\n') to_cut = 1;
	else if (len >= 2 && src[len - 2] == '\\' && src[len - 1] == 'n') to_cut = 2;

	if (len >= (size_t)to_cut) {
		strncpy(dest, src, len - to_cut);
		dest[len - to_cut] = '\0';
	} else {
		dest[0] = '\0';
	}
}

void get_lengths_of_ascii(int *lenest_spaceless, int *lenest_space, const char *str) {
	if (strcmp(str, "arch_linux-default") == 0) {
		int current_width = 0;
		for (int i = 0; arch_linux_default[i] != NULL; i++) {
			if (check_hex(arch_linux_default[i])) continue;

			char cleaned[512];
			copy_without_last_two(cleaned, arch_linux_default[i]);
			current_width += utf8_strlen(cleaned, false);

			if (check_end_newline(arch_linux_default[i])) {
				if (current_width > *lenest_space) *lenest_space = current_width;
				current_width = 0;
			}
		}
	}
}

int check_hex(const char *str) {
	if (!str || str[0] != '#') return 0;
	int len = 0;
	while (str[len] != '\0' && !isspace((unsigned char)str[len])) {
		len++;
	}
	return (len == 7 || len == 4);
}

void free_info_list(void) {
	for (int i = 0; i < info_count; i++) {
		free(info_list[i]);
	}
	free(info_list);
	info_list = NULL;
	info_count = 0;
}

void add_info_line(const char *fmt, ...) {
	va_list args;
	char buffer[512];

	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	char **tmp = realloc(info_list, (info_count + 1) * sizeof(char *));
	if (tmp == NULL) {
		return;
	}
	info_list = tmp;

	info_list[info_count] = strdup(buffer);
	info_count++;
}

void form_info_list() {
        char gpu_name[256];
        char cpu_name[256];
        char host_name[256];
        char uptime[64];
        char memory[64];
        char wm[128];
        char username[128];
        char hostname[128];
        
        char os[256] = {0}, kernel[256] = {0}, shell[64] = {0}, monitor[128] = {0}, terminal[64] = {0};

        get_os(os);
	get_kernel(kernel);
	get_shell(shell);
	get_monitor(monitor);
	get_terminal(terminal);
        
        get_hostname(hostname);
        get_username(username);

        add_info_line("#ff0000");
        add_info_line("%s@", username);
        add_info_line("#ffffff");
        add_info_line("%s\n", hostname);
        
        int gpu_count = get_gpu_count();
        int cpu_count = get_cpu_count();

        for (int i = 0; i < gpu_count; i++) {
                get_gpu_name(gpu_name, i);
                add_info_line("#ff0000");
                add_info_line("GPU:      ");
                add_info_line("#ffffff");
                add_info_line("%s\n", gpu_name);
        }

        for (int i = 0; i < cpu_count; i++) {
                get_cpu_name(cpu_name, i);
                add_info_line("#ff0000");
                add_info_line("CPU:      ");
                add_info_line("#ffffff");
                add_info_line("%s\n", cpu_name);
        }

        get_host_name(host_name);
        add_info_line("#ff0000");
        add_info_line("Host:     ");
        add_info_line("#ffffff");
        add_info_line("%s\n", host_name);

        get_uptime(uptime);
        add_info_line("#ff0000");
        add_info_line("Uptime:   ");
        add_info_line("#ffffff");
        add_info_line("%s\n", uptime);

        get_memory(memory);
        add_info_line("#ff0000");
        add_info_line("Memory:   ");
        add_info_line("#ffffff");
        add_info_line("%s\n", memory);

        get_wm(wm);
        add_info_line("#ff0000");
        add_info_line("WM:       ");
        add_info_line("#ffffff");
        add_info_line("%s\n", wm);

        add_info_line("#ff0000");
        add_info_line("OS:       ");
        add_info_line("#ffffff");
        add_info_line("%s\n", os);

        add_info_line("#ff0000");
	add_info_line("Kernel:   ");
        add_info_line("#ffffff");
	add_info_line("%s\n", kernel);

        add_info_line("#ff0000");
	add_info_line("Packages: ");
        add_info_line("#ffffff");
	add_info_line("%d\n", get_package_count());

        add_info_line("#ff0000");
	add_info_line("Shell:    ");
        add_info_line("#ffffff");
	add_info_line("%s\n", shell);

        add_info_line("#ff0000");
	add_info_line("Monitor:  ");
        add_info_line("#ffffff");
	add_info_line("%s\n", monitor);

        add_info_line("#ff0000");
	add_info_line("Terminal: ");
        add_info_line("#ffffff");
	add_info_line("%s\n", terminal);

        int disk_count = get_disk_count();
        
        char disk_name[256];
        char disk_storage[64];
        char disk_free[64];
        char disk_occupied[64];
        char disk_percent[16];
        for (int i = 0; i < disk_count - 1; i++) {
                if (i < disk_count) {
                        get_disk_name(disk_name, i);
                        get_disk_storage(disk_storage, i);
                        get_disk_free(disk_free, i);
                        get_disk_occupied(disk_occupied, i);
                        get_disk_percent(disk_percent, i);
                        add_info_line("#ff0000");
                        add_info_line("Disk %d:   ", i + 1);
                        add_info_line("#ffffff");
                        add_info_line("%s - %s total, %s free (%s used)\n", disk_name, disk_storage, disk_free, disk_percent);
                }
        }
}

void utf8_ncpy(char *dest, const char *src, int n) {
	int count = 0;
	int i = 0;
	int j = 0;
	while (src[i] && count < n) {
		dest[j++] = src[i++];
		while (src[i] && (src[i] & 0xC0) == 0x80) {
			dest[j++] = src[i++];
		}
		count++;
	}
	dest[j] = '\0';
}

void print_clipped(const char *color, const char *text, int *current_w, int max_w, bool *dots) {
	if (*dots) return;
	
	int len = utf8_strlen(text, false);
	int limit = max_w - 3;
	
	if (*current_w + len <= max_w) {
		if (color) hex_printf(color, "%s", text);
		else printf("%s", text);
		*current_w += len;
		return;
	}
	
	int available = limit - *current_w;
	if (available < 0) available = 0;
	
	char buf[1024];
	utf8_ncpy(buf, text, available);
	
	if (color) hex_printf(color, "%s", buf);
	else printf("%s", buf);
	
	printf("...");
	*dots = true;
	*current_w = max_w;
}