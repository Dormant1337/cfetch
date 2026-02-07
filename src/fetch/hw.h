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

#ifdef __cplusplus
}
#endif

#endif /* FETCH_HW_H */
