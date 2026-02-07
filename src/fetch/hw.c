#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

int get_gpu_count(void) {
        int count = 0;
	DIR *dir = opendir("/sys/class/drm");

	if (!dir) return 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "card", 4) == 0 &&
		    isdigit(entry->d_name[4]) &&
		    strchr(entry->d_name, '-') == NULL) {
			count++;
		}
	}

	closedir(dir);
	return count;
}

void parse_gpu_model(char *s) {
	char *p;

	if ((p = strstr(s, " (rev "))) *p = '\0';

	if ((p = strstr(s, "Advanced Micro Devices, Inc. [AMD/ATI]"))) {
		char tmp[256];
		snprintf(tmp, sizeof(tmp), "AMD %s", p + 39);
		strcpy(s, tmp);
	}

	if ((p = strstr(s, "NVIDIA Corporation"))) {
		char *b_open = strchr(s, '[');
		char *b_close = strchr(s, ']');

		if (b_open && b_close && b_close > b_open) {
			char model[128];
			int len = b_close - b_open - 1;
			strncpy(model, b_open + 1, len);
			model[len] = '\0';
			sprintf(s, "NVIDIA %s", model);
		} else {
			char tmp[256];
			sprintf(tmp, "NVIDIA %s", p + 19);
			strcpy(s, tmp);
		}
	}

	if ((p = strstr(s, " Corporation"))) {
		memmove(p, p + 12, strlen(p + 12) + 1);
	}
}

void get_gpu_name(char *gpu, int number) {
	char path[256];
	char link[256];
	char pci_addr[32] = {0};

	snprintf(path, sizeof(path), "/sys/class/drm/card%d/device", number);
	ssize_t len = readlink(path, link, sizeof(link) - 1);

	if (len != -1) {
		link[len] = '\0';
		char *last_slash = strrchr(link, '/');
		if (last_slash) {
			strncpy(pci_addr, last_slash + 1, sizeof(pci_addr) - 1);
		}
	}

	if (pci_addr[0] == '\0') {
		strcpy(gpu, "Unknown GPU");
		return;
	}

	char cmd[128];
	char buffer[256];
	snprintf(cmd, sizeof(cmd), "lspci -s %s", pci_addr);

	FILE *fp = popen(cmd, "r");
	if (fp) {
		if (fgets(buffer, sizeof(buffer), fp)) {
			char *name_start = strchr(buffer, ':');
			if (name_start) {
				name_start++;
				char *real_name = strchr(name_start, ':');
				if (real_name) name_start = real_name + 1;
				
				while (*name_start == ' ') name_start++;
				name_start[strcspn(name_start, "\n")] = 0;
				
				parse_gpu_model(name_start);
				strcpy(gpu, name_start);
			} else {
				strcpy(gpu, "Generic GPU");
			}
		}
		pclose(fp);
	}
}

void parse_cpu_model(char *s) {
	char *p;

	while ((p = strstr(s, "(R)"))) memmove(p, p + 3, strlen(p + 3) + 1);
	while ((p = strstr(s, "(TM)"))) memmove(p, p + 4, strlen(p + 4) + 1);

	if ((p = strstr(s, " CPU @"))) *p = '\0';

	const char *garbage[] = {" Processor", " 6-Core", " 8-Core", " 12-Core", " 16-Core", " Quad-Core"};
	for (int i = 0; i < 6; i++) {
		if ((p = strstr(s, garbage[i]))) *p = '\0';
	}

	char *src = s, *dst = s;
	while (*src) {
		*dst = *src++;
		if (*dst != ' ' || (*src != ' ' && *src != '\0')) dst++;
	}
	*dst = '\0';

	while (*s == ' ') memmove(s, s + 1, strlen(s + 1) + 1);
	p = s + strlen(s) - 1;
	while (p > s && isspace(*p)) *p-- = '\0';
}



void get_cpu_name(char *cpu, int number) {
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if (!fp) {
		strcpy(cpu, "Unknown CPU");
		return;
	}

	char line[256];
	char last_model[256] = {0};
	int unique_ids[64];
	int unique_count = 0;
	bool found = false;

	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "model name", 10) == 0) {
			char *start = strchr(line, ':');
			if (start) {
				start++;
				while (*start == ' ') start++;
				start[strcspn(start, "\n")] = 0;
				strcpy(last_model, start);
			}
		}

		if (strncmp(line, "physical id", 11) == 0) {
			char *ptr = strchr(line, ':');
			if (ptr) {
				int id = atoi(ptr + 1);
				int current_index = -1;

				for (int i = 0; i < unique_count; i++) {
					if (unique_ids[i] == id) {
						current_index = i;
						break;
					}
				}

				if (current_index == -1 && unique_count < 64) {
					current_index = unique_count;
					unique_ids[unique_count++] = id;
				}

				if (current_index == number) {
					strcpy(cpu, last_model);
					parse_cpu_model(cpu);
					found = true;
					break;
				}
			}
		}
	}

	if (!found && number == 0 && last_model[0] != '\0') {
		strcpy(cpu, last_model);
		parse_cpu_model(cpu);
		found = true;
	}

	fclose(fp);
	if (!found) strcpy(cpu, "Unknown CPU");
}

int get_cpu_count(void) {
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if (!fp)
		return 1;

	char line[256];
	int ids[64];
	int unique_count = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "physical id", 11) == 0) {
			char *ptr = strchr(line, ':');
			if (ptr) {
				int id = atoi(ptr + 1);
				
				int found = 0;
				for (int i = 0; i < unique_count; i++) {
					if (ids[i] == id) {
						found = 1;
						break;
					}
				}

				if (!found && unique_count < 64) {
					ids[unique_count] = id;
					unique_count++;
				}
			}
		}
	}

	fclose(fp);

	return (unique_count > 0) ? unique_count : 1;
}

