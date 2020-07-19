#ifndef LOG_H_
#define LOG_H_

#define rombp_log_err(MSG, ...) fprintf(stderr, MSG, ##__VA_ARGS__)
#define rombp_log_info(MSG, ...) fprintf(stdout, MSG, ##__VA_ARGS__)

#endif
