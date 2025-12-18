#include "memory_manager.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char* TAG = "MEM_MGR";
static SemaphoreHandle_t memory_mutex = NULL;
static uflake_mem_stats_t mem_stats[3] = {0}; // For each memory type

uflake_result_t uflake_memory_init(void) {
    memory_mutex = xSemaphoreCreateMutex();
    if (!memory_mutex) {
        ESP_LOGE(TAG, "Failed to create memory mutex");
        return UFLAKE_ERROR_MEMORY;
    }
    
    // Initialize statistics
    mem_stats[UFLAKE_MEM_INTERNAL].total_size = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    mem_stats[UFLAKE_MEM_SPIRAM].total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    mem_stats[UFLAKE_MEM_DMA].total_size = heap_caps_get_total_size(MALLOC_CAP_DMA);
    
    ESP_LOGI(TAG, "Memory manager initialized - Internal: %d bytes, SPIRAM: %d bytes, DMA: %d bytes",
             (int)mem_stats[UFLAKE_MEM_INTERNAL].total_size,
             (int)mem_stats[UFLAKE_MEM_SPIRAM].total_size,
             (int)mem_stats[UFLAKE_MEM_DMA].total_size);
    
    return UFLAKE_OK;
}

void* uflake_malloc(size_t size, uflake_mem_type_t type) {
    if (!size || type >= 3) return NULL;
    
    xSemaphoreTake(memory_mutex, portMAX_DELAY);
    
    uint32_t caps = 0;
    switch (type) {
        case UFLAKE_MEM_INTERNAL: caps = MALLOC_CAP_INTERNAL; break;
        case UFLAKE_MEM_SPIRAM: caps = MALLOC_CAP_SPIRAM; break;
        case UFLAKE_MEM_DMA: caps = MALLOC_CAP_DMA; break;
    }
    
    void* ptr = heap_caps_malloc(size, caps);
    if (ptr) {
        mem_stats[type].allocations++;
        mem_stats[type].used_size += size;
        mem_stats[type].free_size = heap_caps_get_free_size(caps);
        mem_stats[type].largest_free_block = heap_caps_get_largest_free_block(caps);
    }
    
    xSemaphoreGive(memory_mutex);
    return ptr;
}

void uflake_free(void* ptr) {
    if (!ptr) return;
    
    xSemaphoreTake(memory_mutex, portMAX_DELAY);
    
    // Update statistics (simplified - we don't track which type was used)
    for (int i = 0; i < 3; i++) {
        mem_stats[i].deallocations++;
    }
    
    heap_caps_free(ptr);
    xSemaphoreGive(memory_mutex);
}

uflake_result_t uflake_memory_get_stats(uflake_mem_type_t type, uflake_mem_stats_t* stats) {
    if (!stats || type >= 3) return UFLAKE_ERROR_INVALID_PARAM;
    
    xSemaphoreTake(memory_mutex, portMAX_DELAY);
    *stats = mem_stats[type];
    xSemaphoreGive(memory_mutex);
    
    return UFLAKE_OK;
}

void* uflake_calloc(size_t count, size_t size, uflake_mem_type_t type) {
    size_t total_size = count * size;
    void* ptr = uflake_malloc(total_size, type);
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    return ptr;
}

void* uflake_realloc(void* ptr, size_t size) {
    if (!ptr) {
        return uflake_malloc(size, UFLAKE_MEM_INTERNAL);
    }
    
    if (size == 0) {
        uflake_free(ptr);
        return NULL;
    }
    
    xSemaphoreTake(memory_mutex, portMAX_DELAY);
    void* new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL);
    xSemaphoreGive(memory_mutex);
    
    return new_ptr;
}

size_t uflake_memory_get_free_size(uflake_mem_type_t type) {
    uint32_t caps = 0;
    switch (type) {
        case UFLAKE_MEM_INTERNAL: caps = MALLOC_CAP_INTERNAL; break;
        case UFLAKE_MEM_SPIRAM: caps = MALLOC_CAP_SPIRAM; break;
        case UFLAKE_MEM_DMA: caps = MALLOC_CAP_DMA; break;
        default: return 0;
    }
    
    return heap_caps_get_free_size(caps);
}
