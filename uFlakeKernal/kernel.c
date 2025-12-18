#include "kernel.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

static const char *TAG = "KERNEL";
static uflake_kernel_t g_kernel = {0};

// Kernel main task
static void kernel_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Kernel task started");

    while (g_kernel.state == KERNEL_STATE_RUNNING)
    {
        // Update tick count
        g_kernel.tick_count++;

        // Run scheduler
        uflake_scheduler_tick();

        // Process timer callbacks
        uflake_timer_process();

        // Process message queues
        uflake_messagequeue_process();

        // Process events
        uflake_event_process();

        // Check watchdog
        uflake_watchdog_feed();

        // Check for panic conditions
        uflake_panic_check();

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGW(TAG, "Kernel task exiting");
    vTaskDelete(NULL);
}

uflake_result_t uflake_kernel_init(void)
{
    ESP_LOGI(TAG, "Initializing uFlake Kernel v1.0");

    if (g_kernel.state != KERNEL_STATE_UNINITIALIZED)
    {
        return UFLAKE_ERROR;
    }

    g_kernel.state = KERNEL_STATE_INITIALIZING;

    // Create kernel mutex
    g_kernel.kernel_mutex = xSemaphoreCreateMutex();
    if (!g_kernel.kernel_mutex)
    {
        ESP_LOGE(TAG, "Failed to create kernel mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    // Initialize subsystems in order
    ESP_LOGI(TAG, "Initializing memory manager...");
    if (uflake_memory_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Memory manager initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing panic handler...");
    if (uflake_panic_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Panic handler initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing logger...");
    if (uflake_logger_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Logger initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing synchronization...");
    if (uflake_sync_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Synchronization initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing scheduler...");
    if (uflake_scheduler_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Scheduler initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing crypto engine...");
    if (uflake_crypto_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Crypto engine initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing buffer manager...");
    if (uflake_buffer_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Buffer manager initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing timer manager...");
    if (uflake_timer_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Timer manager initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing message queue system...");
    if (uflake_messagequeue_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Message queue system initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing watchdog manager...");
    if (uflake_watchdog_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Watchdog manager initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing event manager...");
    if (uflake_event_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Event manager initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Initializing resource manager...");
    if (uflake_resource_init() != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Resource manager initialization failed");
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Kernel initialization completed successfully");
    return UFLAKE_OK;
}

uflake_result_t uflake_kernel_start(void)
{
    if (g_kernel.state != KERNEL_STATE_INITIALIZING)
    {
        return UFLAKE_ERROR;
    }

    // Create kernel task
    BaseType_t result = xTaskCreate(
        kernel_task,
        "uflake_kernel",
        UFLAKE_KERNEL_STACK_SIZE,
        NULL,
        UFLAKE_KERNEL_PRIORITY,
        &g_kernel.kernel_task);

    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create kernel task");
        return UFLAKE_ERROR_MEMORY;
    }

    g_kernel.state = KERNEL_STATE_RUNNING;
    ESP_LOGI(TAG, "Kernel started successfully");

    return UFLAKE_OK;
}

uflake_result_t uflake_kernel_shutdown(void)
{
    if (g_kernel.state != KERNEL_STATE_RUNNING)
    {
        return UFLAKE_ERROR;
    }

    ESP_LOGI(TAG, "Shutting down uFlake Kernel...");

    // Set state to stop kernel task
    g_kernel.state = KERNEL_STATE_UNINITIALIZED;

    // Wait for kernel task to exit
    if (g_kernel.kernel_task)
    {
        vTaskDelay(pdMS_TO_TICKS(100)); // Give time for task to exit
        g_kernel.kernel_task = NULL;
    }

    // Cleanup kernel mutex
    if (g_kernel.kernel_mutex)
    {
        vSemaphoreDelete(g_kernel.kernel_mutex);
        g_kernel.kernel_mutex = NULL;
    }

    ESP_LOGI(TAG, "Kernel shutdown completed");
    return UFLAKE_OK;
}

kernel_state_t uflake_kernel_get_state(void)
{
    return g_kernel.state;
}

uint32_t uflake_kernel_get_tick_count(void)
{
    return g_kernel.tick_count;
}
