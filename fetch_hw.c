#define _POSIX_C_SOURCE 200809L
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <ctype.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <time.h>
#include <sys/statvfs.h>

#include "fetch_hw.h" 

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif


char *xstrdup(const char *s)
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

char *get_monitor_info(void)
{
	FILE *pipe;
	char line[256];
	char *res = NULL;
	char *tmp;

	pipe = popen("xrandr --current | grep '*' | awk '{print $1\" @ \"$2\"Hz\"}'", "r");
	if (!pipe)
		return xstrdup("unknown");
	if (fgets(line, sizeof(line), pipe) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		res = strdup(line);
	}
	pclose(pipe);
	if (!res) {
		tmp = strdup("unknown");
		return tmp;
	}
	return res;
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



char *get_disk_info(void)
{
	struct statvfs fs;
	unsigned long total, used;
	double total_gb, used_gb;
	char *res;

	if (statvfs("/", &fs) != 0)
		return xstrdup("unknown");

	total = fs.f_blocks * fs.f_frsize;
	used = (fs.f_blocks - fs.f_bfree) * fs.f_frsize;
	total_gb = (double) total / (1024.0*1024*1024);
	used_gb = (double) used / (1024.0*1024*1024);

	res = malloc(64);
	if (!res) return xstrdup("unknown");
	snprintf(res, 64, "%.1fG / %.1fG", used_gb, total_gb);
	return res;
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