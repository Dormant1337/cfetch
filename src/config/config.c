#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#include "toml.h"

int space_between_ascii_and_info = 1;
bool auto_shorten = true;

void create_default_config(const char *path) {
	FILE *fp = fopen(path, "w");
	if (fp) {
		fprintf(fp, "space_between_ascii_and_info = 1\n");
		fprintf(fp, "auto_shorten = true\n");
		fclose(fp);
	}
}

void init_config(void) {
	char path[1024];
	const char *home = getenv("HOME");
	if (!home) return;

	snprintf(path, sizeof(path), "%s/.config", home);
	struct stat st = {0};
	if (stat(path, &st) == -1) {
		mkdir(path, 0755);
	}

	snprintf(path, sizeof(path), "%s/.config/cfetch", home);
	if (stat(path, &st) == -1) {
		mkdir(path, 0755);
	}

	snprintf(path, sizeof(path), "%s/.config/cfetch/config.toml", home);

	if (access(path, F_OK) == -1) {
		create_default_config(path);
		return;
	}

	FILE *fp = fopen(path, "r");
	if (!fp) return;

	char errbuf[200];
	toml_table_t *conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
	fclose(fp);

	if (!conf) return;

	toml_datum_t d_space = toml_int_in(conf, "space_between_ascii_and_info");
	if (d_space.ok) {
		space_between_ascii_and_info = (int)d_space.u.i;
	}

	toml_datum_t d_short = toml_bool_in(conf, "auto_shorten");
	if (d_short.ok) {
		auto_shorten = d_short.u.b;
	}

	toml_free(conf);
}