#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h> 
#include <stdio.h>

/*
 * =====================================================================================
 *
 *       Filename:  config.h
 *
 *    Description:  Data structures and public functions for handling cfetch configuration.
 *
 * =====================================================================================
 */





struct kv_pair {
	char *key;
	char *val;
};

struct line_cfg {
	int present;
	char *format;
	char *color;
	char *label_color;
	char *data_color;
	struct kv_pair *forced;
	size_t forced_count;
	int arrange_box;
};

struct cfetch_cfg {
	char *ascii_art;
	char *default_label_color;
	char *default_data_color;
	int info_padding;
	struct line_cfg *lines;
	size_t lines_count;
	char *frame_type;
	char *frame_color;
	char *frame_title_soft;
	char *frame_title_hard;
};



/**
 * @brief Initializes a config struct with default values.
 * @param cfg Pointer to the cfetch_cfg struct to initialize.
 */
void cfg_init_defaults(struct cfetch_cfg *cfg);

/**
 * @brief Loads configuration from the default file path into the struct.
 * @param cfg Pointer to the cfetch_cfg struct to fill.
 * @return 1 on success, 0 on failure (e.g., file not found).
 */
int cfg_load(struct cfetch_cfg *cfg);

/**
 * @brief Frees all dynamically allocated memory within a config struct.
 * @param cfg Pointer to the cfetch_cfg struct to clean up.
 */
void cfg_free(struct cfetch_cfg *cfg);

/**
 * @brief Gets a forced (overridden) value for a placeholder in a specific line.
 *        Used by the display logic to check for overrides.
 * @param lc Pointer to the line_cfg struct.
 * @param key The name of the placeholder (e.g., "os").
 * @return A dynamically allocated string with the value, or NULL if not found.
 *         The caller is responsible for freeing the returned string.
 */
char *line_force_get_dup(const struct line_cfg *lc, const char *key);

typedef struct {
    char **lines;  
    int line_count;  
    int capacity;   
} CustomAsciiArt;


extern CustomAsciiArt g_custom_art;
void add_custom_ascii_line(const char* line);
void parse_custom_ascii_section(FILE *file);


#endif // CONFIG_H