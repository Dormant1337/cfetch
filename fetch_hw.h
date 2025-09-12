#ifndef FETCH_H
#define FETCH_H

/*
 * =====================================================================================
 *
 *       Filename:  fetch.h
 *
 *    Description:  Public interface for fetching system hardware information.
 *
 * =====================================================================================
 */

/**
 * @brief Fetches the name and specifications of the primary GPU.
 * @return A dynamically allocated string with the GPU info, or NULL on failure.
 *         The caller is responsible for freeing this memory.
 */
char *get_gpu_name(void);

/**
 * @brief Fetches the current memory usage (used/total).
 * @return A dynamically allocated string with memory info (e.g., "8.1/15.8 GB"),
 *         or NULL on failure. The caller is responsible for freeing this memory.
 */
char *get_memory(void);

/**
 * @brief Fetches the resolution and refresh rate of the primary monitor.
 * @return A dynamically allocated string with monitor info, or NULL on failure.
 *         The caller is responsible for freeing this memory.
 */
char *get_monitor_info(void);


/* --- Utility Functions --- */

/**
 * @brief A safe string duplication function.
 * @param s The string to duplicate.
 * @return A dynamically allocated copy of the string, or NULL on failure.
 *         The caller is responsible for freeing this memory.
 */
char *xstrdup(const char *s);

char *get_motherboard(void);

char *get_disk_info(void);

char *get_cpu_name(void);


#endif // FETCH_H