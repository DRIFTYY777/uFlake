#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "kernel.h"
#include <esp_random.h>

static const char *TAG = "MAIN";

// Example application processes
static void sensor_process(void *args);
static void display_process(void *args);
static void communication_process(void *args);
static void crypto_process(void *args);

// Timer callbacks
static void heartbeat_timer(void *args);
static void data_collection_timer(void *args);

// Event callbacks
static void system_event_handler(const void *event_data);
static void error_event_handler(const void *event_data);

// Global variables for demonstration
static uflake_msgqueue_t *sensor_queue = NULL;
static uflake_msgqueue_t *display_queue = NULL;
static uflake_buffer_t *shared_buffer = NULL;
static uflake_mutex_t *shared_mutex = NULL;
static uint32_t heartbeat_timer_id = 0;
static uint32_t data_timer_id = 0;

extern "C"
{
    void app_main(void)
    {
        ESP_LOGI(TAG, "uFlake OS Demo - Starting Advanced Kernel Example...");

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

        ESP_LOGI(TAG, "uFlake Kernel started successfully");

        // Wait for kernel to be fully running
        vTaskDelay(pdMS_TO_TICKS(100));

        // === DEMONSTRATE KERNEL SUBSYSTEMS ===

        // 1. Create Message Queues for IPC
        ESP_LOGI(TAG, "Creating message queues for IPC...");
        uflake_msgqueue_create("sensor_data", 10, true, &sensor_queue);
        uflake_msgqueue_create("display_cmd", 5, false, &display_queue);

        // 2. Create Shared Resources
        ESP_LOGI(TAG, "Creating shared resources...");
        uflake_buffer_create(&shared_buffer, 1024);
        uflake_mutex_create(&shared_mutex);

        // 3. Create Processes
        ESP_LOGI(TAG, "Creating application processes...");
        uint32_t sensor_pid, display_pid, comm_pid, crypto_pid;

        uflake_process_create("sensor_proc", sensor_process, NULL,
                              2048, PROCESS_PRIORITY_HIGH, &sensor_pid);

        uflake_process_create("display_proc", display_process, NULL,
                              2048, PROCESS_PRIORITY_NORMAL, &display_pid);

        uflake_process_create("comm_proc", communication_process, NULL,
                              4096, PROCESS_PRIORITY_LOW, &comm_pid);

        uflake_process_create("crypto_proc", crypto_process, NULL,
                              3072, PROCESS_PRIORITY_CRITICAL, &crypto_pid);

        // 4. Create Timers
        ESP_LOGI(TAG, "Creating system timers...");
        uflake_timer_create(&heartbeat_timer_id, 1000, heartbeat_timer, NULL, true);
        uflake_timer_create(&data_timer_id, 5000, data_collection_timer, NULL, true);

        uflake_timer_start(heartbeat_timer_id);
        uflake_timer_start(data_timer_id);

        // 5. Subscribe to Events
        ESP_LOGI(TAG, "Subscribing to system events...");
        uint32_t sys_sub_id, err_sub_id;
        uflake_event_subscribe(UFLAKE_EVENT_SYSTEM_PANIC, system_event_handler, &sys_sub_id);
        uflake_event_subscribe(UFLAKE_EVENT_MEMORY_LOW, error_event_handler, &err_sub_id);

        // 6. Demonstrate Crypto Operations
        ESP_LOGI(TAG, "Testing crypto operations...");
        uint8_t test_data[] = "Hello uFlake OS!";
        uint8_t hash_output[UFLAKE_SHA256_DIGEST_LENGTH];

        if (uflake_sha256(test_data, strlen((char *)test_data), hash_output) == UFLAKE_OK)
        {
            ESP_LOGI(TAG, "SHA256 hash computed successfully");
        }

        // 7. Demonstrate Memory Statistics
        uflake_mem_stats_t mem_stats;
        if (uflake_memory_get_stats(UFLAKE_MEM_INTERNAL, &mem_stats) == UFLAKE_OK)
        {
            ESP_LOGI(TAG, "Memory Stats - Used: %d, Free: %d, Allocations: %d",
                     (int)mem_stats.used_size, (int)mem_stats.free_size,
                     (int)mem_stats.allocations);
        }

        // 8. Create Watchdog for Critical Process
        uint32_t watchdog_id;
        uflake_watchdog_create("sensor_watchdog", WATCHDOG_TYPE_TASK, 10000, &watchdog_id);

        ESP_LOGI(TAG, "uFlake OS Demo fully initialized - All subsystems active!");
        ESP_LOGI(TAG, "Processes: %d, Timers: 2, Queues: 2, Events: 2", 4);

        // Main application loop
        while (true)
        {
            // Publish periodic system events
            static uint32_t counter = 0;
            if (++counter % 30 == 0)
            { // Every 30 seconds
                uflake_event_publish("app.status", EVENT_TYPE_USER, &counter, sizeof(counter));
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// === PROCESS IMPLEMENTATIONS ===

static void sensor_process(void *args)
{
    ESP_LOGI("SENSOR", "Sensor process started");

    uint32_t sensor_data = 0;
    uflake_message_t msg;

    while (true)
    {
        // Simulate sensor reading
        sensor_data = esp_random() % 100;

        // Prepare message
        msg.sender_pid = 1; // Simplified PID
        msg.type = MSG_TYPE_DATA;
        msg.priority = MSG_PRIORITY_NORMAL;
        msg.data_size = sizeof(sensor_data);
        memcpy(msg.data, &sensor_data, sizeof(sensor_data));

        // Send to display process
        if (sensor_queue)
        {
            uflake_msgqueue_send(sensor_queue, &msg, 1000);
            ESP_LOGD("SENSOR", "Sent sensor data: %d", (int)sensor_data);
        }

        // Use shared buffer with mutex protection
        if (shared_mutex && shared_buffer)
        {
            uflake_mutex_lock(shared_mutex, 1000);

            char buffer_data[64];
            snprintf(buffer_data, sizeof(buffer_data), "Sensor: %d", (int)sensor_data);
            uflake_buffer_write(shared_buffer, buffer_data, strlen(buffer_data));

            uflake_mutex_unlock(shared_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void display_process(void *args)
{
    ESP_LOGI("DISPLAY", "Display process started");

    uflake_message_t received_msg;

    while (true)
    {
        // Check for messages from sensor
        if (sensor_queue)
        {
            if (uflake_msgqueue_receive(sensor_queue, &received_msg, 500) == UFLAKE_OK)
            {
                uint32_t sensor_value;
                memcpy(&sensor_value, received_msg.data, sizeof(sensor_value));
                ESP_LOGI("DISPLAY", "Displaying sensor value: %d", (int)sensor_value);

                // Trigger display update event
                uflake_event_publish("display.updated", EVENT_TYPE_USER, &sensor_value, sizeof(sensor_value));
            }
        }

        // Read shared buffer
        if (shared_mutex && shared_buffer)
        {
            uflake_mutex_lock(shared_mutex, 1000);

            char read_buffer[64] = {0};
            uflake_buffer_read(shared_buffer, read_buffer, sizeof(read_buffer));
            if (strlen(read_buffer) > 0)
            {
                ESP_LOGD("DISPLAY", "Shared buffer: %s", read_buffer);
            }

            uflake_mutex_unlock(shared_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void communication_process(void *args)
{
    ESP_LOGI("COMM", "Communication process started");

    uflake_buffer_t *tx_buffer = NULL;
    uflake_buffer_create(&tx_buffer, 512);

    while (true)
    {
        // Simulate network communication
        char comm_data[] = "Network packet data";

        if (tx_buffer)
        {
            uflake_buffer_write(tx_buffer, comm_data, strlen(comm_data));
            ESP_LOGD("COMM", "Prepared network packet: %s", comm_data);

            // Simulate transmission
            vTaskDelay(pdMS_TO_TICKS(100));

            // Clear buffer after transmission
            uflake_buffer_resize(tx_buffer, 512); // Reset buffer
        }

        // Broadcast system status
        uflake_message_t broadcast_msg = {0};
        broadcast_msg.type = MSG_TYPE_BROADCAST;
        broadcast_msg.priority = MSG_PRIORITY_LOW;
        strcpy((char *)broadcast_msg.data, "System OK");
        broadcast_msg.data_size = strlen("System OK");

        uflake_msgqueue_broadcast(&broadcast_msg);

        vTaskDelay(pdMS_TO_TICKS(10000)); // Every 10 seconds
    }
}

static void crypto_process(void *args)
{
    ESP_LOGI("CRYPTO", "Crypto process started");

    uflake_aes_context_t aes_ctx;
    uint8_t test_key[UFLAKE_AES_KEY_LENGTH] = {0}; // Simplified key
    uint8_t test_iv[UFLAKE_AES_BLOCK_SIZE] = {0};  // Simplified IV

    // Initialize crypto context
    memcpy(aes_ctx.key, test_key, UFLAKE_AES_KEY_LENGTH);
    memcpy(aes_ctx.iv, test_iv, UFLAKE_AES_BLOCK_SIZE);

    while (true)
    {
        // Generate random data
        uint8_t random_data[32];
        uflake_random_bytes(random_data, sizeof(random_data));
        ESP_LOGD("CRYPTO", "Generated %d bytes of random data", sizeof(random_data));

        // Compute hash
        uint8_t hash[UFLAKE_SHA256_DIGEST_LENGTH];
        if (uflake_sha256(random_data, sizeof(random_data), hash) == UFLAKE_OK)
        {
            ESP_LOGD("CRYPTO", "Computed SHA256 hash");
        }

        // Demonstrate encryption (simplified - real implementation needs proper padding)
        uint8_t plaintext[16] = {'T', 'e', 's', 't', ' ', 'e', 'n', 'c', 'r', 'y', 'p', 't', 'i', 'o', 'n', '!'};
        uint8_t ciphertext[16];
        uint8_t decrypted[16];

        if (uflake_aes_encrypt(&aes_ctx, plaintext, 16, ciphertext) == UFLAKE_OK)
        {
            ESP_LOGD("CRYPTO", "AES encryption successful");

            if (uflake_aes_decrypt(&aes_ctx, ciphertext, 16, decrypted) == UFLAKE_OK)
            {
                ESP_LOGD("CRYPTO", "AES decryption successful");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(15000)); // Every 15 seconds
    }
}

// === TIMER CALLBACKS ===

static void heartbeat_timer(void *args)
{
    static uint32_t heartbeat_count = 0;
    heartbeat_count++;

    ESP_LOGI("HEARTBEAT", "System heartbeat #%d - Kernel tick: %d",
             (int)heartbeat_count, (int)uflake_kernel_get_tick_count());

    // Publish heartbeat event
    uflake_event_publish("system.heartbeat", EVENT_TYPE_SYSTEM, &heartbeat_count, sizeof(heartbeat_count));
}

static void data_collection_timer(void *args)
{
    ESP_LOGI("DATA_TIMER", "Periodic data collection triggered");

    // Collect system statistics
    size_t free_heap = esp_get_free_heap_size();
    uflake_mem_stats_t stats;

    if (uflake_memory_get_stats(UFLAKE_MEM_INTERNAL, &stats) == UFLAKE_OK)
    {
        ESP_LOGI("DATA_TIMER", "Heap: %d bytes, uFlake allocations: %d",
                 (int)free_heap, (int)stats.allocations);
    }

    // Check for low memory condition
    if (free_heap < 10240)
    { // Less than 10KB
        uflake_event_publish(UFLAKE_EVENT_MEMORY_LOW, EVENT_TYPE_ERROR, &free_heap, sizeof(free_heap));
    }
}

// === EVENT HANDLERS ===

static void system_event_handler(const void *event_data)
{
    ESP_LOGW("EVENT", "System event received - taking corrective action");

    // In a real system, this might:
    // - Save critical data to flash
    // - Gracefully shutdown non-essential processes
    // - Trigger system recovery procedures
}

static void error_event_handler(const void *event_data)
{
    const uint32_t *free_memory = (const uint32_t *)event_data;
    ESP_LOGE("EVENT", "Low memory event: %d bytes remaining", (int)*free_memory);

    // Trigger memory cleanup
    // Request processes to free unnecessary resources
}
