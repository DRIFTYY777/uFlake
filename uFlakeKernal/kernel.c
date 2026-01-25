#include "kernel.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "rom/ets_sys.h"

static const char *TAG = "KERNEL";
static uflake_kernel_t g_kernel = {0};

// Hardware timer interrupt handler
void vApplicationTickHook(void)
{
    static uint32_t tick_counter = 0;
    tick_counter++;

// Debug: Log every 10 seconds to confirm tick hook is working
#ifdef CONFIG_LOG_DEFAULT_LEVEL_DEBUG
    static uint32_t debug_counter = 0;
    debug_counter++;
    if (debug_counter >= 10000) // 10 seconds at 1000Hz
    {
        debug_counter = 0;
        // Note: Don't use ESP_LOG from interrupt context
    }
#endif
}

// Kernel main task - The "uFlake Kernel" core
static void kernel_task(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    ESP_LOGI(TAG, "Kernel subscribed to hardware watchdog (exclusive)");

    // Main kernel loop - equivalent to OS scheduler main loop
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

        // Check software watchdog timeouts (uflake_watchdog_t instances)
        uflake_watchdog_check_timeouts();

        // Feed hardware watchdog
        esp_task_wdt_reset();

        // Check for panic conditions
        uflake_panic_check();

        // Small delay to yield CPU
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
    }

    // Unsubscribe from watchdog before exiting
    esp_task_wdt_delete(NULL);

    ESP_LOGW(TAG, "Kernel task exiting");
    vTaskDelete(NULL);
}

uflake_result_t uflake_kernel_init(void)
{
    ESP_LOGI(TAG, "Initializing uFlake Kernel v1.3");

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

    // Set state to RUNNING BEFORE creating task to avoid race condition
    g_kernel.state = KERNEL_STATE_RUNNING;

    BaseType_t result = xTaskCreate(
        kernel_task,
        "uFlake_OS_Kernel", // Clear name indicating this is the OS kernel
        UFLAKE_KERNEL_STACK_SIZE,
        NULL,
        configMAX_PRIORITIES - 2, // High priority but not absolute maximum
        &g_kernel.kernel_task);

    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create kernel task");
        g_kernel.state = KERNEL_STATE_INITIALIZING; // Restore state on failure
        return UFLAKE_ERROR_MEMORY;
    }

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

// Hardware timer-based delay functions
void uflake_kernel_delay(uint32_t ticks)
{
    if (uflake_kernel_is_in_isr())
    {
        ESP_LOGE(TAG, "Cannot delay from ISR context");
        return;
    }
    vTaskDelay(ticks);
}
