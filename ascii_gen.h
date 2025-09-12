#ifndef ASCII_GEN_H
#define ASCII_GEN_H
#include "config.h"

/*
 * =====================================================================================
 *
 *       Filename:  ascii_gen.h
 *
 *    Description:  Functions related to ASCII art generation and exporting.
 *
 * =====================================================================================
 */

struct art_row {
	int cstart;
	int start;
	int end;
	size_t visual_len;
};

extern char last_hexcolor[16];


/**
 * @brief Reads an ASCII art file and prints it to stdout as a C string array.
 * @param filename The path to the ASCII art file.
 * @return 0 on success, 1 on failure.
 */


int export_ascii_art(const char *filename);
int hex_to_true_color(const char *hex_color);
void print_info_line_idx(size_t idx, const struct cfetch_cfg *cfg, const char *restore_color);
char *line_render_plain(const struct line_cfg *lc);
void print_art_row_segments(const char *art[], int start, int end, char *latest_hex_color, size_t latest_hex_color_sz);
void build_art_rows(const char *art[], struct art_row **out_rows, size_t *out_count, size_t *out_max);
void print_repeat_char(const char *color_hex, const char *restore_hex, char ch, size_t count);
void print_repeat_utf8(const char *color_hex, const char *restore_hex, const char *glyph, size_t count);
static void print_titled_border_line(const char *left_ch, const char *right_ch, const char *title, size_t inner_width, const char *frame_color, const char *restore_color);
int frame_kind(const struct cfetch_cfg *cfg);
const char *frame_color_or_default(const struct cfetch_cfg *cfg);
void print_ascii_configured(const char *art[], const struct cfetch_cfg *cfg);
const char **auto_art_by_distro(char *distro);
const char **select_art_by_name(const char *name, char *distro);
#endif // ASCII_GEN_H