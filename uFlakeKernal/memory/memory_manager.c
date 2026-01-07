#include "memory_manager.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "string.h"
#include "soc/soc_caps.h"
#include "esp_memory_utils.h"

static const char *TAG = "MEM_MGR";
static SemaphoreHandle_t memory_mutex = NULL;
static uflake_mem_stats_t mem_stats[3] = {0}; // For each memory type

// Track allocation metadata for better statistics
typedef struct
{
    void *ptr;
    size_t size;
    uflake_mem_type_t type;
} mem_allocation_t;

#define MAX_TRACKED_ALLOCS 256
static mem_allocation_t tracked_allocs[MAX_TRACKED_ALLOCS] = {0};

uflake_result_t uflake_memory_init(void)
{
    memory_mutex = xSemaphoreCreateMutex();
    if (!memory_mutex)
    {
        ESP_LOGE(TAG, "Failed to create memory mutex");
        return UFLAKE_ERROR_MEMORY;
    }

    // Initialize statistics
    mem_stats[UFLAKE_MEM_INTERNAL].total_size = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    mem_stats[UFLAKE_MEM_INTERNAL].free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    mem_stats[UFLAKE_MEM_INTERNAL].largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

    mem_stats[UFLAKE_MEM_SPIRAM].total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    mem_stats[UFLAKE_MEM_SPIRAM].free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    mem_stats[UFLAKE_MEM_SPIRAM].largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    mem_stats[UFLAKE_MEM_DMA].total_size = heap_caps_get_total_size(MALLOC_CAP_DMA);
    mem_stats[UFLAKE_MEM_DMA].free_size = heap_caps_get_free_size(MALLOC_CAP_DMA);
    mem_stats[UFLAKE_MEM_DMA].largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);

    ESP_LOGI(TAG, "=== Memory Manager Initialized ===");
    ESP_LOGI(TAG, "Internal RAM: Total=%u Free=%u Largest=%u",
             (unsigned)mem_stats[UFLAKE_MEM_INTERNAL].total_size,
             (unsigned)mem_stats[UFLAKE_MEM_INTERNAL].free_size,
             (unsigned)mem_stats[UFLAKE_MEM_INTERNAL].largest_free_block);

    if (mem_stats[UFLAKE_MEM_SPIRAM].total_size > 0)
    {
        ESP_LOGI(TAG, "PSRAM (SPIRAM): Total=%u Free=%u Largest=%u",
                 (unsigned)mem_stats[UFLAKE_MEM_SPIRAM].total_size,
                 (unsigned)mem_stats[UFLAKE_MEM_SPIRAM].free_size,
                 (unsigned)mem_stats[UFLAKE_MEM_SPIRAM].largest_free_block);
        ESP_LOGI(TAG, "PSRAM MMU Integration: ENABLED");
    }
    else
    {
        ESP_LOGW(TAG, "PSRAM: NOT AVAILABLE or NOT ENABLED");
    }

    ESP_LOGI(TAG, "DMA-capable RAM: Total=%u Free=%u Largest=%u",
             (unsigned)mem_stats[UFLAKE_MEM_DMA].total_size,
             (unsigned)mem_stats[UFLAKE_MEM_DMA].free_size,
             (unsigned)mem_stats[UFLAKE_MEM_DMA].largest_free_block);

    return UFLAKE_OK;
}

void *uflake_malloc(size_t size, uflake_mem_type_t type)
{
    if (!size || type >= 3)
        return NULL;

    xSemaphoreTake(memory_mutex, portMAX_DELAY);

    uint32_t caps = 0;
    const char *type_name = "UNKNOWN";
    switch (type)
    {
    case UFLAKE_MEM_INTERNAL:
        caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
        type_name = "INTERNAL";
        break;
    case UFLAKE_MEM_SPIRAM:
        caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        type_name = "SPIRAM";
        break;
    case UFLAKE_MEM_DMA:
        caps = MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
        type_name = "DMA";
        break;
    }

    void *ptr = heap_caps_malloc(size, caps);
    if (ptr)
    {
        mem_stats[type].allocations++;
        mem_stats[type].used_size += size;
        mem_stats[type].free_size = heap_caps_get_free_size(caps);
        mem_stats[type].largest_free_block = heap_caps_get_largest_free_block(caps);

        // Track allocation for better statistics
        for (int i = 0; i < MAX_TRACKED_ALLOCS; i++)
        {
            if (tracked_allocs[i].ptr == NULL)
            {
                tracked_allocs[i].ptr = ptr;
                tracked_allocs[i].size = size;
                tracked_allocs[i].type = type;
                break;
            }
        }

        // Check if pointer is in PSRAM range (for verification)
        bool is_in_psram = esp_ptr_external_ram(ptr);

        ESP_LOGD(TAG, "Allocated %u bytes from %s at %p (in_psram=%d)",
                 (unsigned)size, type_name, ptr, is_in_psram);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to allocate %u bytes from %s (free=%u largest=%u)",
                 (unsigned)size, type_name,
                 (unsigned)heap_caps_get_free_size(caps),
                 (unsigned)heap_caps_get_largest_free_block(caps));
    }

    xSemaphoreGive(memory_mutex);
    return ptr;
}

void uflake_free(void *ptr)
{
    if (!ptr)
        return;

    xSemaphoreTake(memory_mutex, portMAX_DELAY);

    // Find and remove from tracked allocations
    for (int i = 0; i < MAX_TRACKED_ALLOCS; i++)
    {
        if (tracked_allocs[i].ptr == ptr)
        {
            uflake_mem_type_t type = tracked_allocs[i].type;
            size_t size = tracked_allocs[i].size;

            mem_stats[type].deallocations++;
            mem_stats[type].used_size -= size;

            // Clear tracking entry
            tracked_allocs[i].ptr = NULL;
            tracked_allocs[i].size = 0;
            tracked_allocs[i].type = 0;

            ESP_LOGD(TAG, "Freed %u bytes at %p", (unsigned)size, ptr);
            break;
        }
    }

    heap_caps_free(ptr);
    xSemaphoreGive(memory_mutex);
}

uflake_result_t uflake_memory_get_stats(uflake_mem_type_t type, uflake_mem_stats_t *stats)
{
    if (!stats || type >= 3)
        return UFLAKE_ERROR_INVALID_PARAM;

    xSemaphoreTake(memory_mutex, portMAX_DELAY);
    *stats = mem_stats[type];
    xSemaphoreGive(memory_mutex);

    return UFLAKE_OK;
}

void *uflake_calloc(size_t count, size_t size, uflake_mem_type_t type)
{
    size_t total_size = count * size;
    void *ptr = uflake_malloc(total_size, type);
    if (ptr)
    {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void *uflake_realloc(void *ptr, size_t size)
{
    if (!ptr)
    {
        return uflake_malloc(size, UFLAKE_MEM_INTERNAL);
    }

    if (size == 0)
    {
        uflake_free(ptr);
        return NULL;
    }

    xSemaphoreTake(memory_mutex, portMAX_DELAY);
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL);
    xSemaphoreGive(memory_mutex);

    return new_ptr;
}

size_t uflake_memory_get_free_size(uflake_mem_type_t type)
{
    uint32_t caps = 0;
    switch (type)
    {
    case UFLAKE_MEM_INTERNAL:
        caps = MALLOC_CAP_INTERNAL;
        break;
    case UFLAKE_MEM_SPIRAM:
        caps = MALLOC_CAP_SPIRAM;
        break;
    case UFLAKE_MEM_DMA:
        caps = MALLOC_CAP_DMA;
        break;
    default:
        return 0;
    }

    return heap_caps_get_free_size(caps);
}

bool uflake_memory_is_psram_available(void)
{
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
}

void uflake_memory_print_stats(void)
{
    ESP_LOGI(TAG, "=== Memory Statistics ===");

    const char *type_names[] = {"INTERNAL", "SPIRAM", "DMA"};
    for (int i = 0; i < 3; i++)
    {
        ESP_LOGI(TAG, "%s: Total=%u Used=%u Free=%u Allocs=%u Deallocs=%u Largest=%u",
                 type_names[i],
                 (unsigned)mem_stats[i].total_size,
                 (unsigned)mem_stats[i].used_size,
                 (unsigned)mem_stats[i].free_size,
                 (unsigned)mem_stats[i].allocations,
                 (unsigned)mem_stats[i].deallocations,
                 (unsigned)mem_stats[i].largest_free_block);
    }

    // Print system-wide heap info
    ESP_LOGI(TAG, "System Heap: Free=%u MinFree=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
}
