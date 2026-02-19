#define _DEFAULT_SOURCE

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/utsname.h>



void get_os(char *os) {
	FILE *fp = fopen("/etc/os-release", "r");
	if (fp) {
		char line[256];
		while (fgets(line, sizeof(line), fp)) {
			if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
				char *name = line + 13;
				name[strcspn(name, "\"\n")] = 0;
				strcpy(os, name);
				break;
			}
		}
		fclose(fp);
	}
}

void get_kernel(char *kernel) {
	struct utsname buffer;
	if (uname(&buffer) == 0) {
		strcpy(kernel, buffer.release);
	}
}

int get_package_count(void) {
	int count = 0;
	DIR *dir;
	struct dirent *entry;

	if ((dir = opendir("/var/lib/pacman/local"))) {
		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_type == DT_DIR && entry->d_name[0] != '.') count++;
		}
		closedir(dir);
		return count - 1;
	}

	FILE *fp = popen("dpkg-query -f '${binary:Package}\\n' -W 2>/dev/null | wc -l", "r");
	if (fp) {
		if (fscanf(fp, "%d", &count) != 1) count = 0;
		pclose(fp);
	}
	return count;
}

void get_shell(char *shell) {
	char *sh = getenv("SHELL");
	if (sh) {
		char *slash = strrchr(sh, '/');
		strcpy(shell, slash ? slash + 1 : sh);
	} else {
		strcpy(shell, "unknown");
	}
}

void get_terminal(char *terminal) {
	char *term = getenv("TERM_PROGRAM");
	if (!term) term = getenv("TERMINAL_EMULATOR");
	if (!term) term = getenv("TERM");
	
	if (term) {
		strcpy(terminal, term);
	} else {
		strcpy(terminal, "unknown");
	}
}

void get_wm(char *wm) {
	char *desktop = getenv("XDG_CURRENT_DESKTOP");
	if (desktop) {
		strcpy(wm, desktop);
	} else {
		char *wm_env = getenv("WINDOWMANAGER");
		if (wm_env)
			strcpy(wm, wm_env);
		else
			strcpy(wm, "Unknown");
	}
}

void get_uptime(char *uptime) {
	FILE *fp = fopen("/proc/uptime", "r");
	double total_seconds;

	if (fp) {
		if (fscanf(fp, "%lf", &total_seconds) == 1) {
			int h = (int)total_seconds / 3600;
			int m = ((int)total_seconds % 3600) / 60;
			if (h > 0)
				sprintf(uptime, "%dh %dm", h, m);
			else
				sprintf(uptime, "%dm", m);
		}
		fclose(fp);
	}
}

void get_username(char *username) {
	char *user = getenv("USER");
	if (user) {
		strcpy(username, user);
	} else {
		strcpy(username, "unknown");
	}
}

void get_hostname(char *hostname) {
	if (gethostname(hostname, 256) != 0) {
		strcpy(hostname, "unknown");
	}
}