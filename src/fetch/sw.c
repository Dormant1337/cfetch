#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

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