#ifndef FETCH_HW_H
#define FETCH_HW_H

#ifdef __cplusplus
extern "C" {
#endif



int  get_gpu_count(void);
void parse_gpu_model(char *s);
void get_gpu_name(char *gpu, int number);

int  get_cpu_count(void);
void parse_cpu_model(char *s);
void get_cpu_name(char *cpu, int number);

void get_host_name(char *host);
void get_memory(char *memory);

void get_monitor(char *monitor);

int get_disk_count(void);
void get_disk_name(char *disk, int number);

void get_disk_storage(char *output, int number);
void get_disk_free(char *output, int number);
void get_disk_occupied(char *output, int number);
void get_disk_percent(char *output, int number);

#ifdef __cplusplus
}
#endif

#endif /* FETCH_HW_H */
