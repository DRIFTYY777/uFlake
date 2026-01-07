#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "kernel.h"
#include "uI2c.h"
#include "uSPI.h"
#include "unvs.h"

#include "pca9555.h"
#include "sdCard.h"
#include "nrf24.h"

static const char *TAG = "MAIN";

// Process ID for uFlake kernel process management demonstration
static uint32_t input_process_pid = 0;

// Event subscription IDs for demonstration
static uint32_t system_event_sub_id = 0;
static uint32_t memory_event_sub_id = 0;
static uint32_t custom_event_sub_id = 0;

void input_read_task(void *arg)
{
    ESP_LOGI(TAG, "[INPUT_TASK] Starting input read task");

    // initialize PCA9555 as input
    init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);

    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for PCA9555 to stabilize

    ESP_LOGI(TAG, "[INPUT_TASK] Initialization complete, entering main loop");

    while (1)
    {
        uint16_t inputs_value = read_pca9555_inputs(UI2C_PORT_0, PCA9555_ADDRESS);
        if (!((inputs_value >> 0) & 0x01))
        {
            ESP_LOGI(TAG, "_Up pressed");
        }

        if (!((inputs_value >> 1) & 0x01))
        {
            ESP_LOGI(TAG, "_Down pressed");
        }

        if (!((inputs_value >> 2) & 0x01))
        {
            ESP_LOGI(TAG, "_Right pressed");
        }

        if (!((inputs_value >> 3) & 0x01))
        {
            ESP_LOGI(TAG, "_Left pressed");
        }

        if (!((inputs_value >> 4) & 0x01))
        {
            // Serial.println("isnt used"); // isnt used
        }

        if (!((inputs_value >> 5) & 0x01))
        {
            ESP_LOGI(TAG, "_Menu pressed");
        }

        if (!((inputs_value >> 6) & 0x01))
        {
            ESP_LOGI(TAG, "_Back pressed");
        }

        if (!((inputs_value >> 7) & 0x01))
        {
            ESP_LOGI(TAG, "_OK pressed");
        }

        if (!((inputs_value >> 8) & 0x01))
        {
            ESP_LOGI(TAG, "_Home pressed");
        }

        if (!((inputs_value >> 9) & 0x01))
        {
            ESP_LOGI(TAG, "_A pressed");
        }
        if (!((inputs_value >> 10) & 0x01))
        {
            ESP_LOGI(TAG, "_B pressed");
        }

        if (!((inputs_value >> 11) & 0x01))
        {
            ESP_LOGI(TAG, "_Y pressed");
        }
        if (!((inputs_value >> 12) & 0x01))
        {
            ESP_LOGI(TAG, "_X pressed");
        }

        if (!((inputs_value >> 13) & 0x01))
        {
            ESP_LOGI(TAG, "_L1 pressed");
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100 ms
    }
}

// ============================================================================
// uFlake Kernel Process Management Demo: The Benefits of Kernel Abstraction
// ============================================================================
/*
 * WHY USE UFLAKE KERNEL INSTEAD OF RAW FREERTOS?
 *
 * 1. ABSTRACTION: Higher-level APIs hide FreeRTOS complexity
 * 2. PORTABILITY: Easy to switch RTOS underneath (FreeRTOS → Zephyr → etc)
 * 3. SAFETY: Kernel validates operations, tracks resources
 * 4. FEATURES: Process tracking, CPU time accounting, debugging
 * 5. CONSISTENCY: Unified API across all kernel services
 */
void demo_kernel_process_management(void)
{
    ESP_LOGI(TAG, "\n\n=== uFlake Kernel Process Management Demo ===");
    ESP_LOGI(TAG, "Using KERNEL APIs (not raw FreeRTOS)\n");

    // Step 1: Create process using uFlake kernel
    ESP_LOGI(TAG, "[STEP 1] Creating process via uFlake kernel...");
    ESP_LOGI(TAG, "  API: uflake_process_create()");

    uflake_result_t result = uflake_process_create(
        "InputReader",           // Process name
        input_read_task,         // Entry point
        NULL,                    // Arguments
        4096,                    // Stack size
        PROCESS_PRIORITY_NORMAL, // Kernel priority enum
        &input_process_pid       // Returns PID
    );

    if (result != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create process! Error: %d", result);
        return;
    }
    ESP_LOGI(TAG, "✓ Process created: PID=%u", (unsigned)input_process_pid);
    ESP_LOGI(TAG, "  Kernel automatically: tracks state, manages memory, logs lifecycle");

    // Let process run
    ESP_LOGI(TAG, "\n[STEP 2] Letting process run for 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Step 2: Suspend using kernel API
    ESP_LOGI(TAG, "\n[STEP 3] Suspending process via kernel...");
    ESP_LOGI(TAG, "  API: uflake_process_suspend(pid)");

    result = uflake_process_suspend(input_process_pid);
    if (result == UFLAKE_OK)
    {
        ESP_LOGW(TAG, "✓ Process SUSPENDED by kernel");
        ESP_LOGI(TAG, "  Kernel benefits:");
        ESP_LOGI(TAG, "    - Validates PID exists");
        ESP_LOGI(TAG, "    - Updates process state tracking");
        ESP_LOGI(TAG, "    - Logs operation for debugging");
    }
    else if (result == UFLAKE_ERROR_INVALID_PARAM)
    {
        ESP_LOGE(TAG, "✗ Cannot suspend: process already terminated");
        ESP_LOGI(TAG, "  This demonstrates kernel's safety checks!");
        return;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to suspend process: %d", result);
        return;
    }

    // Wait while suspended
    ESP_LOGI(TAG, "\n[STEP 4] Process suspended for 5 seconds...");
    for (int i = 5; i > 0; i--)
    {
        ESP_LOGI(TAG, "  Resuming in %d seconds...", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Step 3: Resume using kernel API
    ESP_LOGI(TAG, "\n[STEP 5] Resuming process via kernel...");
    ESP_LOGI(TAG, "  API: uflake_process_resume(pid)");

    result = uflake_process_resume(input_process_pid);
    if (result == UFLAKE_OK)
    {
        ESP_LOGI(TAG, "✓ Process RESUMED by kernel");
        ESP_LOGI(TAG, "  Kernel automatically updated process state");
    }

    // Let it run
    ESP_LOGI(TAG, "\n[STEP 6] Letting process run for 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Step 4: Suspend again
    ESP_LOGI(TAG, "\n[STEP 7] Second suspend test...");
    uflake_process_suspend(input_process_pid);
    ESP_LOGW(TAG, "✓ Process SUSPENDED again");
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Step 5: Resume again
    ESP_LOGI(TAG, "\n[STEP 8] Final resume...");
    uflake_process_resume(input_process_pid);
    ESP_LOGI(TAG, "✓ Process RESUMED - continuing operation");

    ESP_LOGI(TAG, "\n=== Demo Complete ===");
    ESP_LOGI(TAG, "\nKERNEL vs RAW FREERTOS COMPARISON:");
    ESP_LOGI(TAG, "┌─────────────────────┬──────────────────────┬─────────────────────┐");
    ESP_LOGI(TAG, "│ Feature             │ Raw FreeRTOS         │ uFlake Kernel       │");
    ESP_LOGI(TAG, "├─────────────────────┼──────────────────────┼─────────────────────┤");
    ESP_LOGI(TAG, "│ API Complexity      │ xTaskCreate(7 args)  │ process_create(6)   │");
    ESP_LOGI(TAG, "│ Process Tracking    │ Manual               │ Automatic           │");
    ESP_LOGI(TAG, "│ State Management    │ Manual               │ Kernel-managed      │");
    ESP_LOGI(TAG, "│ Error Validation    │ None                 │ Built-in            │");
    ESP_LOGI(TAG, "│ Debugging Support   │ Limited              │ Full logging        │");
    ESP_LOGI(TAG, "│ Portability         │ FreeRTOS-specific    │ RTOS-agnostic       │");
    ESP_LOGI(TAG, "│ Memory Safety       │ Manual tracking      │ Kernel-tracked      │");
    ESP_LOGI(TAG, "└─────────────────────┴──────────────────────┴─────────────────────┘");
    ESP_LOGI(TAG, "\nProcess continues running in background...\n");
}

// ============================================================================
// uFlake Kernel Event System Demo: Publish-Subscribe Pattern
// ============================================================================

// Event callback for system events (process created/terminated)
void on_system_event(const void *event_data)
{
    const uflake_event_t *event = (const uflake_event_t *)event_data;
    ESP_LOGI(TAG, "[EVENT_CALLBACK] System Event: %s, timestamp=%u",
             event->name, (unsigned)event->timestamp);

    if (event->data_size > 0)
    {
        ESP_LOGI(TAG, "[EVENT_CALLBACK]   Data: %u bytes received", (unsigned)event->data_size);
    }
}

// Event callback for memory events
void on_memory_event(const void *event_data)
{
    const uflake_event_t *event = (const uflake_event_t *)event_data;
    ESP_LOGW(TAG, "[EVENT_CALLBACK] ⚠️ Memory Event: %s", event->name);

    if (event->data_size >= sizeof(size_t))
    {
        size_t free_mem = *((size_t *)event->data);
        ESP_LOGW(TAG, "[EVENT_CALLBACK]   Free memory: %u bytes", (unsigned)free_mem);
    }
}

// Event callback for custom user events
void on_custom_event(const void *event_data)
{
    const uflake_event_t *event = (const uflake_event_t *)event_data;
    ESP_LOGI(TAG, "[EVENT_CALLBACK] 🔔 Custom Event: %s", event->name);

    if (event->data_size > 0)
    {
        ESP_LOGI(TAG, "[EVENT_CALLBACK]   Custom data: %.*s",
                 (int)event->data_size, (char *)event->data);
    }
}

/*
 * WHY USE KERNEL EVENT SYSTEM?
 *
 * 1. DECOUPLING: Publishers don't need to know about subscribers
 * 2. FLEXIBILITY: Multiple subscribers can listen to same event
 * 3. ASYNC COMMUNICATION: Non-blocking event delivery
 * 4. DEBUGGING: Centralized event logging and tracking
 * 5. EXTENSIBILITY: Easy to add new event types without changing existing code
 */
void demo_kernel_events(void)
{
    ESP_LOGI(TAG, "\n\n=== uFlake Kernel Event System Demo ===");
    ESP_LOGI(TAG, "Demonstrating Publish-Subscribe Pattern\n");

    // ========================================================================
    // STEP 1: Subscribe to events
    // ========================================================================
    ESP_LOGI(TAG, "[STEP 1] Subscribing to events...");

    // Subscribe to process creation events
    uflake_result_t result = uflake_event_subscribe(
        UFLAKE_EVENT_PROCESS_CREATED,
        on_system_event,
        &system_event_sub_id);
    if (result == UFLAKE_OK)
    {
        ESP_LOGI(TAG, "✓ Subscribed to '%s' (ID: %u)",
                 UFLAKE_EVENT_PROCESS_CREATED, (unsigned)system_event_sub_id);
    }

    // Subscribe to memory low events
    result = uflake_event_subscribe(
        UFLAKE_EVENT_MEMORY_LOW,
        on_memory_event,
        &memory_event_sub_id);
    if (result == UFLAKE_OK)
    {
        ESP_LOGI(TAG, "✓ Subscribed to '%s' (ID: %u)",
                 UFLAKE_EVENT_MEMORY_LOW, (unsigned)memory_event_sub_id);
    }

    // Subscribe to custom user event
    result = uflake_event_subscribe(
        "user.button.pressed",
        on_custom_event,
        &custom_event_sub_id);
    if (result == UFLAKE_OK)
    {
        ESP_LOGI(TAG, "✓ Subscribed to 'user.button.pressed' (ID: %u)",
                 (unsigned)custom_event_sub_id);
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    // ========================================================================
    // STEP 2: Publish events with data
    // ========================================================================
    ESP_LOGI(TAG, "\n[STEP 2] Publishing events...");

    // Publish system event with process data
    ESP_LOGI(TAG, "Publishing: Process created event");
    uint32_t pid_data = 42;
    uflake_event_publish(
        UFLAKE_EVENT_PROCESS_CREATED,
        EVENT_TYPE_SYSTEM,
        &pid_data,
        sizeof(pid_data));
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow time for processing

    // Publish memory event with free size
    ESP_LOGI(TAG, "Publishing: Memory low event");
    size_t free_mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    uflake_event_publish(
        UFLAKE_EVENT_MEMORY_LOW,
        EVENT_TYPE_ERROR,
        &free_mem,
        sizeof(free_mem));
    vTaskDelay(pdMS_TO_TICKS(100));

    // Publish custom user event with string data
    ESP_LOGI(TAG, "Publishing: Custom button press event");
    const char *button_data = "Button A pressed 3x";
    uflake_event_publish(
        "user.button.pressed",
        EVENT_TYPE_USER,
        button_data,
        strlen(button_data));
    vTaskDelay(pdMS_TO_TICKS(100));

    // ========================================================================
    // STEP 3: Publish hardware events
    // ========================================================================
    ESP_LOGI(TAG, "\n[STEP 3] Publishing hardware events...");

    ESP_LOGI(TAG, "Publishing: Multiple custom events");
    for (int i = 0; i < 3; i++)
    {
        char event_name[32];
        snprintf(event_name, sizeof(event_name), "hardware.sensor%d.data", i);

        int sensor_value = esp_random() % 100;
        uflake_event_publish(
            event_name,
            EVENT_TYPE_HARDWARE,
            &sensor_value,
            sizeof(sensor_value));
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    // ========================================================================
    // STEP 4: Event Statistics and Benefits
    // ========================================================================
    ESP_LOGI(TAG, "\n[STEP 4] Event System Benefits:");
    ESP_LOGI(TAG, "┌────────────────────────────────────────────────────────────┐");
    ESP_LOGI(TAG, "│ Feature          │ Description                             │");
    ESP_LOGI(TAG, "├──────────────────┼─────────────────────────────────────────┤");
    ESP_LOGI(TAG, "│ Decoupling       │ Components don't directly depend on     │");
    ESP_LOGI(TAG, "│                  │ each other - only on event names        │");
    ESP_LOGI(TAG, "├──────────────────┼─────────────────────────────────────────┤");
    ESP_LOGI(TAG, "│ Multiple         │ Many subscribers can listen to          │");
    ESP_LOGI(TAG, "│ Subscribers      │ same event (broadcast pattern)          │");
    ESP_LOGI(TAG, "├──────────────────┼─────────────────────────────────────────┤");
    ESP_LOGI(TAG, "│ Async Delivery   │ Events queued and processed in          │");
    ESP_LOGI(TAG, "│                  │ kernel context (non-blocking)           │");
    ESP_LOGI(TAG, "├──────────────────┼─────────────────────────────────────────┤");
    ESP_LOGI(TAG, "│ Type Safety      │ Events carry type info and validated    │");
    ESP_LOGI(TAG, "│                  │ data payloads                           │");
    ESP_LOGI(TAG, "├──────────────────┼─────────────────────────────────────────┤");
    ESP_LOGI(TAG, "│ Debugging        │ Centralized logging of all events       │");
    ESP_LOGI(TAG, "│                  │ with timestamps                         │");
    ESP_LOGI(TAG, "└──────────────────┴─────────────────────────────────────────┘");

    ESP_LOGI(TAG, "\n=== Event Demo Complete ===");
    ESP_LOGI(TAG, "Subscriptions remain active - callbacks will fire on new events\n");
}

void init_nrf24()
{
    NRF24_t nrf24_dev = {
        .cePin = GPIO_NUM_48,
        .csnPin = GPIO_NUM_45,
        .channel = 76,
        .payload = 16,
        .spiHost = USPI_HOST_SPI3,
        .frequency = USPI_FREQ_20MHZ};

    if (!Nrf24_init(&nrf24_dev))
    {
        ESP_LOGE(TAG, "Failed to initialize NRF24L01+");
        return;
    }

    ESP_LOGI(TAG, "NRF24L01+ initialized successfully");

    // check connection
    if (Nrf24_isConnected(&nrf24_dev))
    {
        ESP_LOGI(TAG, "NRF24L01+ is connected");
    }
    else
    {
        ESP_LOGE(TAG, "NRF24L01+ is NOT connected");
    }
}

extern "C"
{
    void app_main(void)
    {

        printf("Internal heap: %zu bytes\n", (size_t)esp_get_free_heap_size());
        printf("PSRAM heap: %zu bytes\n",
               (size_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

        // Initialize the kernel
        if (uflake_kernel_init() != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize uFlake Kernel");
            return;
        }

        // Start the kernel
        if (uflake_kernel_start() != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to start uFlake Kernel");
            return;
        }

        // initialize nvs subsystem
        if (unvs_init() != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize NVS subsystem");
            return;
        }

        // initialize I2C
        if (i2c_bus_manager_init(UI2C_PORT_0, GPIO_NUM_8, GPIO_NUM_9, UI2C_DEFAULT_FREQ_HZ) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize I2C bus");
            return;
        }

        // read and write test
        const char *test_namespace = "test_ns";
        const char *test_key = "test_key";
        const char *test_value = "Hello, uFlake!";
        if (unvs_write_string(test_namespace, test_key, test_value) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write string to NVS");
            return;
        }

        char read_value[50];
        size_t len = sizeof(read_value);
        if (unvs_read_string(test_namespace, test_key, read_value, &len) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read string from NVS");
            return;
        }

        ESP_LOGI(TAG, "uFlake OS started successfully");

        // Demonstrate uFlake kernel process management
        ESP_LOGI(TAG, "\n\n======================================");
        ESP_LOGI(TAG, "uFlake Kernel Process Management Demo");
        ESP_LOGI(TAG, "======================================\n");
        demo_kernel_process_management();

        // INITIALIZE the FIRST SPI BUS  - before adding any devices
        if (uspi_bus_init(USPI_HOST_SPI3, GPIO_NUM_11, GPIO_NUM_13, GPIO_NUM_12, 4096) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize SPI bus");
            return;
        }

        // INITIALIZE the Second SPI BUS FIRST - before adding any devices
        if (uspi_bus_init(USPI_HOST_SPI2, GPIO_NUM_41, GPIO_NUM_38, GPIO_NUM_40, 4096) != UFLAKE_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize SPI bus");
            return;
        }

        // init_nrf24();

        // // initialize SD card
        // SD_CardConfig sd_config = {
        //     .csPin = GPIO_NUM_5,
        //     .clockSpeedHz = USPI_FREQ_1MHZ,
        //     .host = USPI_HOST_SPI2,
        // };

        // if (!sdCard_init(&sd_config))
        // {
        //     ESP_LOGE(TAG, "Failed to initialize SD card");
        //     return;
        // }

        // Create a process using uflake kernel to handle inputs
        // uflake_process_create("InputsProcess", input_read_task, NULL, 2048, PROCESS_PRIORITY_NORMAL, NULL);
    }
}