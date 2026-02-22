#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

extern int space_between_ascii_and_info;
extern bool auto_shorten;

void init_config(void);

#endif