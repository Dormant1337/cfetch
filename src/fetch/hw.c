#define _DEFAULT_SOURCE

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/statvfs.h>
#include <mntent.h>

void format_size(char *output, unsigned long long bytes) {
	double size = bytes;
	const char *units[] = {"B", "KB", "MB", "GB", "TB"};
	int i = 0;
	while (size >= 1024 && i < 4) {
		size /= 1024;
		i++;
	}
	sprintf(output, "%.1f %s", size, units[i]);
}

static int get_device_name(int number, char *dev_name) {
	DIR *dir = opendir("/sys/block");
	if (!dir) return 0;
	struct dirent *entry;
	int current = 0;
	int found = 0;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		if (strncmp(entry->d_name, "loop", 4) == 0) continue;
		if (strncmp(entry->d_name, "ram", 3) == 0) continue;
		if (current == number) {
			strcpy(dev_name, entry->d_name);
			found = 1;
			break;
		}
		current++;
	}
	closedir(dir);
	return found;
}

static int get_mount_stats(int number, struct statvfs *vfs) {
	char dev_name[64];
	if (!get_device_name(number, dev_name)) return 0;

	FILE *mtab = setmntent("/proc/mounts", "r");
	if (!mtab) return 0;

	struct mntent *ent;
	char target_dev[128];
	snprintf(target_dev, sizeof(target_dev), "/dev/%s", dev_name);

	while ((ent = getmntent(mtab)) != NULL) {
		if (strstr(ent->mnt_fsname, target_dev) == ent->mnt_fsname) {
			if (statvfs(ent->mnt_dir, vfs) == 0) {
				endmntent(mtab);
				return 1;
			}
		}
	}
	endmntent(mtab);
	return 0;
}

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

void get_host_name(char *host) {
	char vendor[128] = {0};
	char product[128] = {0};
	char *p;
	FILE *f1 = fopen("/sys/class/dmi/id/board_vendor", "r");
	FILE *f2 = fopen("/sys/class/dmi/id/board_name", "r");

	if (!f1 || !f2) {
		if (f1) fclose(f1);
		if (f2) fclose(f2);
		f1 = fopen("/sys/class/dmi/id/sys_vendor", "r");
		f2 = fopen("/sys/class/dmi/id/product_name", "r");
	}

	if (f1 && f2) {
		fgets(vendor, sizeof(vendor), f1);
		fgets(product, sizeof(product), f2);
		vendor[strcspn(vendor, "\n")] = 0;
		product[strcspn(product, "\n")] = 0;

		if ((p = strstr(vendor, " Technology"))) *p = '\0';
		if ((p = strstr(vendor, " Inc")))       *p = '\0';
		if ((p = strstr(vendor, " Corp")))      *p = '\0';
		if ((p = strstr(vendor, " Co., Ltd")))  *p = '\0';
		if ((p = strstr(vendor, " COMPUTER")))  *p = '\0';

		if (strcmp(vendor, "ASUSTeK") == 0) strcpy(vendor, "ASUS");

		sprintf(host, "%s %s", vendor, product);
	} else {
		strcpy(host, "Unknown");
	}

	if (f1) fclose(f1);
	if (f2) fclose(f2);
}

void get_memory(char *memory) {
	FILE *fp = fopen("/proc/meminfo", "r");
	long total = 0, available = 0;
	char line[256];

	if (fp) {
		while (fgets(line, sizeof(line), fp)) {
			if (strncmp(line, "MemTotal:", 9) == 0)
				sscanf(line + 9, "%ld", &total);
			if (strncmp(line, "MemAvailable:", 13) == 0)
				sscanf(line + 13, "%ld", &available);
		}
		fclose(fp);
	}

	if (total > 0) {
		long used = (total - available) / 1024;
		sprintf(memory, "%ldMiB / %ldMiB", used, total / 1024);
	}
}

void get_monitor(char *monitor) {
	FILE *fp = popen("xrandr --current 2>/dev/null | grep '*' | head -n1", "r");
	if (fp) {
		char line[256];
		if (fgets(line, sizeof(line), fp)) {
			char res[64], hz[64];
			sscanf(line, " %s %s", res, hz);
			for (int i = 0; hz[i]; i++) if (hz[i] == '*') hz[i] = '\0';
			for (int i = 0; hz[i]; i++) if (hz[i] == '+') hz[i] = '\0';
			sprintf(monitor, "%s @ %sHz", res, hz);
		} else {
			strcpy(monitor, "Unknown");
		}
		pclose(fp);
	} else {
		strcpy(monitor, "Unknown");
	}
}

int get_disk_count(void) {
	int count = 0;
	DIR *dir = opendir("/sys/block");
	if (!dir) return 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		if (strncmp(entry->d_name, "loop", 4) == 0) continue;
		if (strncmp(entry->d_name, "ram", 3) == 0) continue;
		count++;
	}

	closedir(dir);
	return count;
}

void get_disk_name(char *disk, int number) {
	DIR *dir = opendir("/sys/block");
	if (!dir) {
		strcpy(disk, "Unknown");
		return;
	}

	struct dirent *entry;
	int current = 0;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		if (strncmp(entry->d_name, "loop", 4) == 0) continue;
		if (strncmp(entry->d_name, "ram", 3) == 0) continue;

		if (current == number) {
			char path[512];
			char buf[256] = {0};
			FILE *fp;

			snprintf(path, sizeof(path), "/sys/block/%s/device/model", entry->d_name);
			fp = fopen(path, "r");
			if (!fp) {
				snprintf(path, sizeof(path), "/sys/block/%s/model", entry->d_name);
				fp = fopen(path, "r");
			}

			if (fp) {
				if (fgets(buf, sizeof(buf), fp)) {
					buf[strcspn(buf, "\n")] = 0;
					char *end = buf + strlen(buf) - 1;
					while (end > buf && isspace((unsigned char)*end)) *end-- = '\0';
					char *start = buf;
					while (*start && isspace((unsigned char)*start)) start++;
					strcpy(disk, start);
				}
				fclose(fp);
			} else {
				strcpy(disk, entry->d_name);
			}
			break;
		}
		current++;
	}

	closedir(dir);
}

void get_disk_storage(char *output, int number) {
	struct statvfs vfs;
	if (get_mount_stats(number, &vfs)) {
		format_size(output, (unsigned long long)vfs.f_blocks * vfs.f_frsize);
	} else {
		strcpy(output, "N/A");
	}
}

void get_disk_free(char *output, int number) {
	struct statvfs vfs;
	if (get_mount_stats(number, &vfs)) {
		format_size(output, (unsigned long long)vfs.f_bavail * vfs.f_frsize);
	} else {
		strcpy(output, "N/A");
	}
}

void get_disk_occupied(char *output, int number) {
	struct statvfs vfs;
	if (get_mount_stats(number, &vfs)) {
		unsigned long long total = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
		unsigned long long free = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
		format_size(output, total - free);
	} else {
		strcpy(output, "N/A");
	}
}

void get_disk_percent(char *output, int number) {
	struct statvfs vfs;
	if (get_mount_stats(number, &vfs)) {
		unsigned long long total = vfs.f_blocks;
		unsigned long long used = vfs.f_blocks - vfs.f_bfree;
		if (total > 0) {
			double percent = (double)used / total * 100.0;
			sprintf(output, "%.1f%%", percent);
		} else {
			strcpy(output, "0%");
		}
	} else {
		strcpy(output, "N/A");
	}
}
