#ifndef UFLAKE_LOGGER_H
#define UFLAKE_LOGGER_H

#include "../kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3,
    LOG_LEVEL_VERBOSE = 4
} log_level_t;

typedef struct {
    uint32_t timestamp;
    log_level_t level;
    char tag[16];
    char message[128];
} log_entry_t;

uflake_result_t uflake_logger_init(void);
void uflake_log(log_level_t level, const char* tag, const char* format, ...);
uflake_result_t uflake_log_set_level(log_level_t level);
uflake_result_t uflake_log_get_entries(log_entry_t* entries, size_t* count);

#define UFLAKE_LOGE(tag, format, ...) uflake_log(LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define UFLAKE_LOGW(tag, format, ...) uflake_log(LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#define UFLAKE_LOGI(tag, format, ...) uflake_log(LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define UFLAKE_LOGD(tag, format, ...) uflake_log(LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#define UFLAKE_LOGV(tag, format, ...) uflake_log(LOG_LEVEL_VERBOSE, tag, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // UFLAKE_LOGGER_H
