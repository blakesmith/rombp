#ifndef LOG_H_
#define LOG_H_

#define rombp_log_err(MSG, ...) fprintf(stderr, MSG, ##__VA_ARGS__)
#ifdef TARGET_RG350
// Disable info logging when on device.
#define rombp_log_info(MSG, ...) {};
#else
#define rombp_log_info(MSG, ...) fprintf(stdout, MSG, ##__VA_ARGS__)
#endif

#endif
