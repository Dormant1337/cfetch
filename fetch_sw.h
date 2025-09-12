#ifndef FETCH_SW_H
#define FETCH_SW_H

/*
 * =====================================================================================
 *
 *       Filename:  fetch_sw.h
 *
 *    Description:  Public interface for fetching system software information.
 *
 * =====================================================================================
 */


/**
 * @brief Fetches the name and version of the current user's shell.
 * @return A dynamically allocated string with the shell info (e.g., "bash 5.2.15"),
 *         or a fallback string on failure. The caller is responsible for freeing
 *         this memory.
 */
char *get_shell_info(void);


/**
 * @brief Fetches the total number of installed packages based on the distribution.
 * @param distro A string representing the Linux distribution (e.g., "arch", "debian").
 * @return A dynamically allocated string with the package count (e.g., "1234 pkgs"),
 *         or an "unknown" string. The caller is responsible for freeing this memory.
 */
char *get_packages_count(const char *distro);


/**
 * @brief Fetches the name of the current Window Manager or Desktop Environment.
 *        Cleans up common suffixes like ".desktop" or ":".
 * @return A dynamically allocated string with the WM/DE name (e.g., "KDE"),
 *         or NULL on failure. The caller is responsible for freeing this memory.
 */
char *get_wm_clean(void);


char *get_distro(void);

char *get_username_str(void);

char *get_hostname_str(void);

char *get_kernel_str(void);

char *get_shell_str(void);

char *get_uptime_str(void);

char *get_os_str(void);

char *get_host_str(void);

char *get_cpu_str(void);

char *get_gpu_str(void);

char *get_ram_str(void);

char *get_wm_str(void);

char *placeholder_value(const char *name);

void capitalize_first(char *s);



#endif // FETCH_SW_H