#include "scheduler.h"
#include "memory_manager.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

static const char *TAG = "SCHEDULER";
static uflake_process_t *process_list = NULL;
static uint32_t next_pid = 1;
static SemaphoreHandle_t scheduler_mutex = NULL;

uflake_result_t uflake_scheduler_init(void)
{
    scheduler_mutex = xSemaphoreCreateMutex();
    if (!scheduler_mutex)
    {
        ESP_LOGE(TAG, "Failed to create scheduler mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    ESP_LOGI(TAG, "Scheduler initialized");
    return UFLAKE_OK;
}

// Wrapper data to pass both process and entry point
typedef struct
{
    uflake_process_t *process;
    process_entry_t entry;
    void *args;
} process_wrapper_args_t;

static void process_wrapper(void *args)
{
    process_wrapper_args_t *wrapper_args = (process_wrapper_args_t *)args;
    uflake_process_t *process = wrapper_args->process;
    process_entry_t entry = wrapper_args->entry;
    void *user_args = wrapper_args->args;

    ESP_LOGI(TAG, "Process %s (PID: %d) started", process->name, (int)process->pid);

    // Free wrapper args now that we've extracted everything
    uflake_free(wrapper_args);

    process->state = PROCESS_STATE_RUNNING;

    // Call the actual user entry point
    entry(user_args);

    // Mark as terminated (no watchdog cleanup needed)
    process->state = PROCESS_STATE_TERMINATED;
    ESP_LOGI(TAG, "Process %s (PID: %d) terminated", process->name, (int)process->pid);
    vTaskDelete(NULL);
}

uflake_result_t uflake_process_create(const char *name, process_entry_t entry, void *args,
                                      size_t stack_size, process_priority_t priority, uint32_t *pid)
{
    if (!name || !entry)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(scheduler_mutex, portMAX_DELAY);

    uflake_process_t *process = (uflake_process_t *)uflake_malloc(sizeof(uflake_process_t), UFLAKE_MEM_INTERNAL);
    if (!process)
    {
        xSemaphoreGive(scheduler_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    // Initialize process
    process->pid = next_pid++;
    strncpy(process->name, name, sizeof(process->name) - 1);
    process->name[sizeof(process->name) - 1] = '\0';
    process->state = PROCESS_STATE_CREATED;
    process->priority = priority;
    process->stack_size = stack_size;
    process->cpu_time = 0;

    // Create wrapper args to pass both process and entry point
    process_wrapper_args_t *wrapper_args = (process_wrapper_args_t *)uflake_malloc(sizeof(process_wrapper_args_t), UFLAKE_MEM_INTERNAL);
    if (!wrapper_args)
    {
        uflake_free(process);
        xSemaphoreGive(scheduler_mutex);
        return UFLAKE_ERROR_MEMORY;
    }
    wrapper_args->process = process;
    wrapper_args->entry = entry;
    wrapper_args->args = args;

    // Create FreeRTOS task
    BaseType_t result = xTaskCreate(
        process_wrapper,
        process->name,
        stack_size / sizeof(StackType_t),
        wrapper_args, // Pass wrapper args, not just process
        priority + 1, // FreeRTOS priority offset
        &process->task_handle);

    if (result != pdPASS)
    {
        uflake_free(process);
        xSemaphoreGive(scheduler_mutex);
        return UFLAKE_ERROR_MEMORY;
    }

    // Add to process list
    process->next = process_list;
    process_list = process;
    process->state = PROCESS_STATE_READY;

    if (pid)
        *pid = process->pid;

    xSemaphoreGive(scheduler_mutex);
    ESP_LOGI(TAG, "Created process %s (PID: %d)", name, (int)process->pid);

    return UFLAKE_OK;
}

void uflake_scheduler_tick(void)
{
    // Update process statistics with timeout to prevent deadlock
    if (xSemaphoreTake(scheduler_mutex, pdMS_TO_TICKS(10)) != pdTRUE)
    {
        // Could not acquire mutex - skip this tick
        return;
    }

    uflake_process_t *current = process_list;
    while (current)
    {
        if (current->state == PROCESS_STATE_RUNNING)
        {
            current->cpu_time++;
        }
        current = current->next;
    }

    xSemaphoreGive(scheduler_mutex);
}

uflake_result_t uflake_process_terminate(uint32_t pid)
{
    xSemaphoreTake(scheduler_mutex, portMAX_DELAY);

    uflake_process_t *current = process_list;
    uflake_process_t *prev = NULL;

    while (current)
    {
        if (current->pid == pid)
        {
            current->state = PROCESS_STATE_TERMINATED;
            if (current->task_handle)
            {
                vTaskDelete(current->task_handle);
            }

            // Remove from list
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                process_list = current->next;
            }

            uflake_free(current);
            xSemaphoreGive(scheduler_mutex);
            ESP_LOGI(TAG, "Terminated process PID: %d", (int)pid);
            return UFLAKE_OK;
        }
        prev = current;
        current = current->next;
    }

    xSemaphoreGive(scheduler_mutex);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_process_suspend(uint32_t pid)
{
    xSemaphoreTake(scheduler_mutex, portMAX_DELAY);

    uflake_process_t *current = process_list;
    while (current)
    {
        if (current->pid == pid)
        {
            // Safety check: don't suspend terminated processes
            if (current->state == PROCESS_STATE_TERMINATED)
            {
                ESP_LOGW(TAG, "Cannot suspend terminated process PID: %d", (int)pid);
                xSemaphoreGive(scheduler_mutex);
                return UFLAKE_ERROR_INVALID_PARAM;
            }

            if (current->task_handle)
            {
                vTaskSuspend(current->task_handle);
                current->state = PROCESS_STATE_BLOCKED;
                ESP_LOGI(TAG, "Suspended process %s (PID: %d)", current->name, (int)pid);
            }
            xSemaphoreGive(scheduler_mutex);
            return UFLAKE_OK;
        }
        current = current->next;
    }

    xSemaphoreGive(scheduler_mutex);
    ESP_LOGE(TAG, "Process PID: %d not found", (int)pid);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_result_t uflake_process_resume(uint32_t pid)
{
    xSemaphoreTake(scheduler_mutex, portMAX_DELAY);

    uflake_process_t *current = process_list;
    while (current)
    {
        if (current->pid == pid)
        {
            // Safety check: don't resume terminated processes
            if (current->state == PROCESS_STATE_TERMINATED)
            {
                ESP_LOGW(TAG, "Cannot resume terminated process PID: %d", (int)pid);
                xSemaphoreGive(scheduler_mutex);
                return UFLAKE_ERROR_INVALID_PARAM;
            }

            if (current->task_handle)
            {
                vTaskResume(current->task_handle);
                current->state = PROCESS_STATE_READY;
                ESP_LOGI(TAG, "Resumed process %s (PID: %d)", current->name, (int)pid);
            }
            xSemaphoreGive(scheduler_mutex);
            return UFLAKE_OK;
        }
        current = current->next;
    }

    xSemaphoreGive(scheduler_mutex);
    ESP_LOGE(TAG, "Process PID: %d not found", (int)pid);
    return UFLAKE_ERROR_NOT_FOUND;
}

uflake_process_t *uflake_process_get_current(void)
{
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();

    xSemaphoreTake(scheduler_mutex, portMAX_DELAY);

    uflake_process_t *current = process_list;
    while (current)
    {
        if (current->task_handle == current_task)
        {
            xSemaphoreGive(scheduler_mutex);
            return current;
        }
        current = current->next;
    }

    xSemaphoreGive(scheduler_mutex);
    return NULL;
}

void uflake_process_yield(uint32_t delay_ms)
{
    if (delay_ms > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    else
    {
        taskYIELD();
    }
}
