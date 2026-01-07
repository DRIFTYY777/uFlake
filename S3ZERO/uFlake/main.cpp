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

// ============================================================================
// MMU + PSRAM: Place large static data in external RAM (requires MMU integration)
// ============================================================================

// Option 1: Use EXT_RAM_BSS_ATTR for zero-initialized data
EXT_RAM_BSS_ATTR static uint8_t large_buffer_psram[1024 * 1024]; // 1MB in PSRAM

// Option 2: Use EXT_RAM_NOINIT_ATTR for uninitialized data (faster boot)
EXT_RAM_NOINIT_ATTR static uint32_t psram_data_array[256 * 1024]; // 1MB

// Option 3: Normal global (will be in internal RAM unless MMU + auto-allocation enabled)
static uint8_t small_buffer[1024]; // 1KB in internal RAM

// Memory test function to verify PSRAM and MMU
void test_memory_allocation()
{
    ESP_LOGI(TAG, "Starting Memory Allocation Tests");

    // Check PSRAM availability
    if (uflake_memory_is_psram_available())
    {
        ESP_LOGI(TAG, "PSRAM is available and configured!");
    }
    else
    {
        ESP_LOGI(TAG, "✗ PSRAM is NOT available");
    }

    // Print initial memory stats
    uflake_memory_print_stats();

    // Test 1: Allocate from Internal RAM

    ESP_LOGI(TAG, "\n--- Test 1: Internal RAM Allocation ---");

    void *internal_mem = uflake_malloc(10240, UFLAKE_MEM_INTERNAL); // 10KB
    if (internal_mem)
    {
        ESP_LOGI(TAG, "Internal RAM Free Size: %u bytes", (unsigned)uflake_memory_get_free_size(UFLAKE_MEM_INTERNAL));
        // Write test pattern
        memset(internal_mem, 0xAA, 10240);
        ESP_LOGI(TAG, "✓ Allocated 10KB from Internal RAM at %p", internal_mem);
    }
    else
    {
        ESP_LOGI(TAG, "Failed to allocate from Internal RAM");
    }

    // Test 2: Allocate from PSRAM (with MMU integration)

    ESP_LOGI(TAG, "\n--- Test 2: PSRAM Allocation (via MMU) ---");

    void *psram_mem_small = uflake_malloc(51200, UFLAKE_MEM_SPIRAM); // 50KB
    if (psram_mem_small)
    {
        ESP_LOGI(TAG, "Allocated 50KB from PSRAM at %p", psram_mem_small);

        // Verify it's actually in PSRAM address range
        if (esp_ptr_external_ram(psram_mem_small))
        {
            ESP_LOGI(TAG, "Confirmed: Pointer is in PSRAM address space");
        }
        else
        {
            ESP_LOGI(TAG, "⚠ Pointer is NOT in PSRAM address space");
        }

        // Write and read test
        memset(psram_mem_small, 0x55, 51200);
        ESP_LOGI(TAG, "Written test pattern to PSRAM");

        // Verify data integrity
        bool data_ok = true;
        uint8_t *check = (uint8_t *)psram_mem_small;
        for (int i = 0; i < 1000; i++)
        {
            if (check[i] != 0x55)
            {
                data_ok = false;
                break;
            }
        }
        if (data_ok)
        {
            ESP_LOGI(TAG, "✓ PSRAM data integrity verified");
        }
        else
        {
            ESP_LOGE(TAG, "✗ PSRAM access verification failed");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Failed to allocate from PSRAM");
    }

    // Test 3: Large PSRAM allocation
    ESP_LOGI(TAG, "\n--- Test 3: Large PSRAM Allocation (1MB) ---");
    void *psram_mem_large = uflake_malloc(1024 * 1024, UFLAKE_MEM_SPIRAM); // 1MB
    if (psram_mem_large)
    {
        ESP_LOGI(TAG, "Allocated 1MB from PSRAM at %p", psram_mem_large);

        // Test MMU transparent access
        uint8_t *large_buf = (uint8_t *)psram_mem_large;
        large_buf[0] = 0x12;
        large_buf[1024 * 512] = 0x34;      // Middle
        large_buf[1024 * 1024 - 1] = 0x56; // End

        if (large_buf[0] == 0x12 && large_buf[1024 * 512] == 0x34 && large_buf[1024 * 1024 - 1] == 0x56)
        {
            ESP_LOGI(TAG, "✓ MMU transparent access working correctly");
        }
        else
        {
            ESP_LOGE(TAG, "✗ MMU access verification failed");
        }
    }
    else
    {
        ESP_LOGE(TAG, "✗ Failed to allocate 1MB from PSRAM");
    }

    // Test 4: DMA-capable memory
    // ESP_LOGI(TAG, \"\\n--- Test 4: DMA-capable Memory Allocation ---\");
    void *dma_mem = uflake_malloc(4096, UFLAKE_MEM_DMA); // 4KB
    if (dma_mem)
    {
        ESP_LOGI(TAG, "✓ Allocated 4KB DMA-capable memory at %p", dma_mem);
        memset(dma_mem, 0xFF, 4096);
        ESP_LOGI(TAG, "✓ Written to DMA memory");
    }
    else
    {
        ESP_LOGE(TAG, "✗ Failed to allocate DMA memory");
    }

    // Print memory stats after allocations
    ESP_LOGI(TAG, "\n=== Memory Stats After Allocations ===");
    uflake_memory_print_stats();

    // Test 5: Free memory
    ESP_LOGI(TAG, "\n--- Test 5: Memory Deallocation ---");
    if (internal_mem)
    {
        uflake_free(internal_mem);
        ESP_LOGI(TAG, "✓ Freed Internal RAM");
    }
    if (psram_mem_small)
    {
        uflake_free(psram_mem_small);
        ESP_LOGI(TAG, "✓ Freed Small PSRAM allocation");
    }
    if (psram_mem_large)
    {
        uflake_free(psram_mem_large);
        ESP_LOGI(TAG, "✓ Freed Large PSRAM allocation");
    }
    if (dma_mem)
    {
        uflake_free(dma_mem);
        ESP_LOGI(TAG, "✓ Freed DMA memory");
    }

    // Final stats
    ESP_LOGI(TAG, "\n=== Memory Stats After Cleanup ===");
    uflake_memory_print_stats();

    ESP_LOGI(TAG, "\n=== Memory Tests Completed ===");
}

// Demonstrate MMU features with PSRAM
void test_mmu_psram_features()
{
    ESP_LOGI(TAG, "\n\n=== MMU + PSRAM Features Demo ===");

    // Test 1: Access global PSRAM variables (MMU makes this transparent)
    ESP_LOGI(TAG, "\n--- Test 1: MMU-Mapped Global Variables ---");
    ESP_LOGI(TAG, "large_buffer_psram address: %p", (void *)large_buffer_psram);
    ESP_LOGI(TAG, "psram_data_array address: %p", (void *)psram_data_array);

    // Check if they're in PSRAM address space
    if (esp_ptr_external_ram(large_buffer_psram))
    {
        ESP_LOGI(TAG, "✓ large_buffer_psram is in PSRAM (via MMU)");
    }
    else
    {
        ESP_LOGW(TAG, "✗ large_buffer_psram NOT in PSRAM (MMU integration disabled?)");
    }

    if (esp_ptr_external_ram(psram_data_array))
    {
        ESP_LOGI(TAG, "✓ psram_data_array is in PSRAM (via MMU)");
    }
    else
    {
        ESP_LOGW(TAG, "✗ psram_data_array NOT in PSRAM (MMU integration disabled?)");
    }

    // Test 2: Write and read from MMU-mapped PSRAM
    ESP_LOGI(TAG, "\n--- Test 2: MMU Transparent Access ---");

    // Write pattern to PSRAM buffer (MMU handles address translation)
    for (int i = 0; i < 1000; i++)
    {
        large_buffer_psram[i] = (uint8_t)(i & 0xFF);
    }
    ESP_LOGI(TAG, "Written 1000 bytes to PSRAM buffer");

    // Read back and verify (no special code needed - MMU does the work)
    bool verify_ok = true;
    for (int i = 0; i < 1000; i++)
    {
        if (large_buffer_psram[i] != (uint8_t)(i & 0xFF))
        {
            verify_ok = false;
            break;
        }
    }

    if (verify_ok)
    {
        ESP_LOGI(TAG, "✓ MMU transparent read/write verified");
    }
    else
    {
        ESP_LOGE(TAG, "✗ MMU access verification failed");
    }

    // Test 3: Compare access speeds (for demonstration)
    ESP_LOGI(TAG, "\n--- Test 3: Access Speed Comparison ---");

    // Internal RAM access
    uint64_t start = esp_timer_get_time();
    for (int i = 0; i < 10000; i++)
    {
        small_buffer[i % 1024] = i & 0xFF;
    }
    uint64_t internal_time = esp_timer_get_time() - start;

    // PSRAM access via MMU
    start = esp_timer_get_time();
    for (int i = 0; i < 10000; i++)
    {
        large_buffer_psram[i % (1024 * 1024)] = i & 0xFF;
    }
    uint64_t psram_time = esp_timer_get_time() - start;

    ESP_LOGI(TAG, "Internal RAM: %llu µs", internal_time);
    ESP_LOGI(TAG, "PSRAM (MMU): %llu µs", psram_time);
    ESP_LOGI(TAG, "Speed ratio: %.2fx slower", (float)psram_time / internal_time);

    ESP_LOGI(TAG, "\n=== MMU + PSRAM Demo Completed ===\n");
}

void input_read_task(void *arg)
{
    // initialize PCA9555 as input
    init_pca9555_as_input(UI2C_PORT_0, PCA9555_ADDRESS);

    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for PCA9555 to stabilize

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

        // Run memory allocation tests
        ESP_LOGI(TAG, "\\n\\n======================================");
        ESP_LOGI(TAG, "Starting PSRAM and MMU Tests");
        ESP_LOGI(TAG, "======================================\\n");
        test_memory_allocation();
        // Test MMU features with PSRAM
        test_mmu_psram_features();
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