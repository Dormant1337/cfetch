#define _POSIX_C_SOURCE 200809L
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>

#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <time.h>
#include <sys/statvfs.h>

#include "fetch_hw.h" 
#include "utils.h"


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



char *get_packages_count(const char *distro)
{
	FILE *pipe;
	char buf[256];
	long count = 0;
	char *cmd = NULL;
	char *res;

	if (!distro)
		return xstrdup("unknown");

	if (!strcmp(distro, "arch") || !strcmp(distro, "archlinux"))
		cmd = "pacman -Qq";
	else if (!strcmp(distro, "debian") || !strcmp(distro, "ubuntu"))
		cmd = "dpkg --get-selections";
	else if (!strcmp(distro, "fedora") || !strcmp(distro, "rhel") || !strcmp(distro, "redhat"))
		cmd = "rpm -qa";
	else
		return xstrdup("unknown");

	pipe = popen(cmd, "r");
	if (!pipe)
		return xstrdup("unknown");

	while (fgets(buf, sizeof(buf), pipe))
		count++;
	pclose(pipe);

	res = malloc(64);
	if (!res) return xstrdup("unknown");
	snprintf(res, 64, "%ld pkgs", count);
	return res;
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






char *get_username_str(void)
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

char *get_hostname_str(void)
{
	char buf[256];

	if (gethostname(buf, sizeof(buf)) != 0)
		return xstrdup("unknown");
	buf[sizeof(buf) - 1] = '\0';
	return xstrdup(buf);
}

char *get_kernel_str(void)
{
	struct utsname u;

	if (uname(&u) != 0)
		return xstrdup("unknown");
	return xstrdup(u.release);
}

char *get_shell_str(void)
{
	const char *s = getenv("SHELL");
	if (s && *s)
		return xstrdup(s);
	return xstrdup("unknown");
}

char *get_uptime_str(void)
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

char *get_os_str(void)
{
	char *d = get_distro();
	if (!d)
		return xstrdup("unknown");
	capitalize_first(d);
	return d;
}

char *get_host_str(void)
{
	return get_motherboard();
}

char *get_cpu_str(void)
{
	return get_cpu_name();
}

char *get_gpu_str(void)
{
	return get_gpu_name();
}

char *get_ram_str(void)
{
	return get_memory();
}

char *get_wm_str(void)
{
	char *w = get_wm_clean();
	if (!w)
		return xstrdup("unknown");
	capitalize_first(w);
	return w;
}

char *placeholder_value(const char *name)
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
	if (strcmp(n, "monitor") == 0) {
		free(n);
		return get_monitor_info();
        }	
	if (strcmp(n, "disk") == 0) {
		free(n);
		return get_disk_info();
        }
        if (strcmp(n, "packages") == 0) {
		free(n);
		char *d = get_distro();
		char *val = get_packages_count(d);
		if (d) free(d);
		return val;
        }
	free(n);
	return xstrdup("");
}
