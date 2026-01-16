#include "logger.h"
#include "memory_manager.h"
#include "esp_log.h"
#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "LOGGER";
static log_level_t current_log_level = LOG_LEVEL_INFO;
static SemaphoreHandle_t log_mutex = NULL;
static log_entry_t *log_buffer = NULL;
static size_t log_buffer_size = 100;
static size_t log_buffer_index = 0;

uflake_result_t uflake_logger_init(void)
{
    log_mutex = xSemaphoreCreateMutex();
    if (!log_mutex)
    {
        ESP_LOGE(TAG, "Failed to create log mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    log_buffer = (log_entry_t *)uflake_malloc(sizeof(log_entry_t) * log_buffer_size, UFLAKE_MEM_INTERNAL);
    if (!log_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate log buffer");
        return UFLAKE_ERROR_MEMORY;
    }

    memset(log_buffer, 0, sizeof(log_entry_t) * log_buffer_size);

    ESP_LOGI(TAG, "Logger initialized with buffer size: %d", (int)log_buffer_size);
    return UFLAKE_OK;
}

void uflake_log(log_level_t level, const char *tag, const char *format, ...)
{
    if (level > current_log_level)
    {
        return;
    }

    if (!log_buffer)
    {
        return;
    }

    // ISR-SAFE: Skip mutex in interrupt context
    bool in_isr = uflake_kernel_is_in_isr();

    if (!in_isr && log_mutex)
    {
        xSemaphoreTake(log_mutex, portMAX_DELAY);
    }

    log_entry_t *entry = &log_buffer[log_buffer_index];
    entry->timestamp = in_isr ? xTaskGetTickCountFromISR() : xTaskGetTickCount();
    entry->level = level;

    strncpy(entry->tag, tag, sizeof(entry->tag) - 1);
    entry->tag[sizeof(entry->tag) - 1] = '\0';

    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, sizeof(entry->message), format, args);
    va_end(args);

    log_buffer_index = (log_buffer_index + 1) % log_buffer_size;

    if (!in_isr && log_mutex)
    {
        xSemaphoreGive(log_mutex);
    }

    // Also output to ESP-IDF log system
    ESP_LOG_LEVEL_LOCAL((esp_log_level_t)level, tag, "%s", entry->message);
}

uflake_result_t uflake_log_set_level(log_level_t level)
{
    if (level > LOG_LEVEL_VERBOSE)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    current_log_level = level;
    ESP_LOGI(TAG, "Log level set to: %d", level);
    return UFLAKE_OK;
}

uflake_result_t uflake_log_get_entries(log_entry_t *entries, size_t *count)
{
    if (!entries || !count)
    {
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    xSemaphoreTake(log_mutex, portMAX_DELAY);

    size_t copy_count = (*count < log_buffer_size) ? *count : log_buffer_size;
    memcpy(entries, log_buffer, sizeof(log_entry_t) * copy_count);
    *count = copy_count;

    xSemaphoreGive(log_mutex);
    return UFLAKE_OK;
}
