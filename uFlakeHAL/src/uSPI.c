#include "uSPI.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "USPI";

// SPI bus state structure
typedef struct
{
    spi_host_device_t host;
    bool is_initialized;
    uflake_mutex_t *mutex;
    gpio_num_t mosi_pin;
    gpio_num_t miso_pin;
    gpio_num_t sclk_pin;
    int max_transfer_size;
    uint8_t device_count;
    struct spi_device_node *device_list;
} uspi_bus_state_t;

// SPI device node for linked list
typedef struct spi_device_node
{
    spi_device_handle_t handle;
    uspi_device_config_t config;
    uint32_t ref_count;
    uint32_t total_transfers;
    uint32_t last_used_tick;
    struct spi_device_node *next;
} spi_device_node_t;

// Global SPI bus states (ESP32-S3 has 2 SPI hosts: SPI2 and SPI3)
static uspi_bus_state_t uspi_buses[SOC_SPI_PERIPH_NUM] = {0};

// ============================================================================
// PRIVATE HELPER FUNCTIONS
// ============================================================================

static spi_device_node_t *find_device_node(uspi_bus_state_t *bus, spi_device_handle_t handle)
{
    spi_device_node_t *current = bus->device_list;
    while (current)
    {
        if (current->handle == handle)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static esp_err_t add_device_to_list(uspi_bus_state_t *bus, spi_device_handle_t handle,
                                    const uspi_device_config_t *config)
{
    spi_device_node_t *node = (spi_device_node_t *)uflake_malloc(
        sizeof(spi_device_node_t), UFLAKE_MEM_INTERNAL);

    if (!node)
    {
        ESP_LOGE(TAG, "Failed to allocate device node");
        return ESP_ERR_NO_MEM;
    }

    node->handle = handle;
    node->config = *config;
    node->ref_count = 1;
    node->total_transfers = 0;
    node->last_used_tick = xTaskGetTickCount();
    node->next = bus->device_list;
    bus->device_list = node;
    bus->device_count++;

    ESP_LOGI(TAG, "Added SPI device '%s' to host %d (total: %d)",
             config->device_name ? config->device_name : "unnamed",
             bus->host, bus->device_count);

    return ESP_OK;
}

static esp_err_t remove_device_from_list(uspi_bus_state_t *bus, spi_device_handle_t handle)
{
    spi_device_node_t *prev = NULL;
    spi_device_node_t *current = bus->device_list;

    while (current)
    {
        if (current->handle == handle)
        {
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                bus->device_list = current->next;
            }

            ESP_LOGI(TAG, "Removed SPI device '%s' from host %d",
                     current->config.device_name ? current->config.device_name : "unnamed",
                     bus->host);

            uflake_free(current);
            bus->device_count--;
            return ESP_OK;
        }
        prev = current;
        current = current->next;
    }

    return ESP_ERR_NOT_FOUND;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

uflake_result_t uspi_bus_init(spi_host_device_t host, gpio_num_t mosi, gpio_num_t miso,
                              gpio_num_t sclk, int max_transfer_sz)
{
    if (host >= SOC_SPI_PERIPH_NUM || host == SPI1_HOST)
    {
        ESP_LOGE(TAG, "Invalid SPI host: %d", host);
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (uspi_buses[host].is_initialized)
    {
        ESP_LOGW(TAG, "SPI host %d already initialized", host);
        return UFLAKE_OK;
    }

    // Create mutex for this bus
    if (uflake_mutex_create(&uspi_buses[host].mutex) != UFLAKE_OK)
    {
        ESP_LOGE(TAG, "Failed to create mutex for SPI host %d", host);
        return UFLAKE_ERROR_MEMORY;
    }

    // Configure SPI bus
    spi_bus_config_t bus_config = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = max_transfer_sz > 0 ? max_transfer_sz : USPI_MAX_TRANSFER_SIZE,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0};

    esp_err_t ret = spi_bus_initialize(host, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        uflake_mutex_destroy(uspi_buses[host].mutex);
        return UFLAKE_ERROR;
    }

    // Save configuration
    uspi_buses[host].host = host;
    uspi_buses[host].is_initialized = true;
    uspi_buses[host].mosi_pin = mosi;
    uspi_buses[host].miso_pin = miso;
    uspi_buses[host].sclk_pin = sclk;
    uspi_buses[host].max_transfer_size = bus_config.max_transfer_sz;
    uspi_buses[host].device_count = 0;
    uspi_buses[host].device_list = NULL;

    ESP_LOGI(TAG, "SPI host %d initialized: MOSI=%d, MISO=%d, SCLK=%d, Max=%d bytes",
             host, mosi, miso, sclk, bus_config.max_transfer_sz);

    return UFLAKE_OK;
}

esp_err_t uspi_bus_deinit(spi_host_device_t host)
{
    if (host >= SOC_SPI_PERIPH_NUM || !uspi_buses[host].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uflake_mutex_lock(uspi_buses[host].mutex, UINT32_MAX);

    // Free all device nodes
    spi_device_node_t *current = uspi_buses[host].device_list;
    while (current)
    {
        spi_device_node_t *next = current->next;
        spi_bus_remove_device(current->handle);
        uflake_free(current);
        current = next;
    }

    // Free SPI bus
    esp_err_t ret = spi_bus_free(host);

    uflake_mutex_unlock(uspi_buses[host].mutex);
    uflake_mutex_destroy(uspi_buses[host].mutex);

    uspi_buses[host].is_initialized = false;
    uspi_buses[host].device_list = NULL;
    uspi_buses[host].device_count = 0;

    ESP_LOGI(TAG, "SPI host %d deinitialized", host);
    return ret;
}

esp_err_t uspi_device_add(spi_host_device_t host, const uspi_device_config_t *dev_config,
                          spi_device_handle_t *out_handle)
{
    if (host >= SOC_SPI_PERIPH_NUM || !uspi_buses[host].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!dev_config || !out_handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (uspi_buses[host].device_count >= USPI_MAX_DEVICES_PER_BUS)
    {
        ESP_LOGE(TAG, "Maximum devices (%d) reached on host %d",
                 USPI_MAX_DEVICES_PER_BUS, host);
        return ESP_ERR_NO_MEM;
    }

    uflake_mutex_lock(uspi_buses[host].mutex, UINT32_MAX);

    // Configure SPI device
    spi_device_interface_config_t devcfg = {
        .command_bits = dev_config->command_bits,
        .address_bits = dev_config->address_bits,
        .dummy_bits = dev_config->dummy_bits,
        .mode = dev_config->mode,
        .clock_speed_hz = dev_config->clock_speed_hz,
        .spics_io_num = dev_config->cs_pin,
        .queue_size = dev_config->queue_size > 0 ? dev_config->queue_size : 7,
        .flags = 0,
        .pre_cb = NULL,
        .post_cb = NULL};

    if (dev_config->cs_ena_pretrans)
    {
        devcfg.flags |= SPI_DEVICE_NO_DUMMY;
    }

    esp_err_t ret = spi_bus_add_device(host, &devcfg, out_handle);
    if (ret != ESP_OK)
    {
        uflake_mutex_unlock(uspi_buses[host].mutex);
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Add to device list
    ret = add_device_to_list(&uspi_buses[host], *out_handle, dev_config);

    uflake_mutex_unlock(uspi_buses[host].mutex);

    return ret;
}

esp_err_t uspi_device_remove(spi_device_handle_t handle)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Find which bus this device belongs to
    for (int host = 0; host < SOC_SPI_PERIPH_NUM; host++)
    {
        if (!uspi_buses[host].is_initialized)
            continue;

        uflake_mutex_lock(uspi_buses[host].mutex, UINT32_MAX);

        if (find_device_node(&uspi_buses[host], handle))
        {
            esp_err_t ret = spi_bus_remove_device(handle);
            if (ret == ESP_OK)
            {
                remove_device_from_list(&uspi_buses[host], handle);
            }
            uflake_mutex_unlock(uspi_buses[host].mutex);
            return ret;
        }

        uflake_mutex_unlock(uspi_buses[host].mutex);
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t uspi_device_acquire_bus(spi_device_handle_t handle, TickType_t wait)
{
    return spi_device_acquire_bus(handle, wait);
}

void uspi_device_release_bus(spi_device_handle_t handle)
{
    spi_device_release_bus(handle);
}

// ============================================================================
// TRANSFER OPERATIONS
// ============================================================================

esp_err_t uspi_transmit(spi_device_handle_t handle, const uint8_t *tx_buffer,
                        size_t length, uint32_t timeout_ms)
{
    if (!handle || !tx_buffer || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .length = length * 8, // Length in bits
        .tx_buffer = tx_buffer,
        .rx_buffer = NULL};

    esp_err_t ret = spi_device_transmit(handle, &trans);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t uspi_receive(spi_device_handle_t handle, uint8_t *rx_buffer,
                       size_t length, uint32_t timeout_ms)
{
    if (!handle || !rx_buffer || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .length = length * 8,
        .rxlength = length * 8,
        .tx_buffer = NULL,
        .rx_buffer = rx_buffer};

    esp_err_t ret = spi_device_transmit(handle, &trans);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI receive failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t uspi_transfer(spi_device_handle_t handle, const uint8_t *tx_buffer,
                        uint8_t *rx_buffer, size_t length, uint32_t timeout_ms)
{
    if (!handle || !tx_buffer || !rx_buffer || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .length = length * 8,
        .rxlength = length * 8,
        .tx_buffer = tx_buffer,
        .rx_buffer = rx_buffer};

    esp_err_t ret = spi_device_transmit(handle, &trans);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI transfer failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t uspi_queue_trans(spi_device_handle_t handle, spi_transaction_t *trans_desc, TickType_t ticks_to_wait)
{
    return spi_device_queue_trans(handle, trans_desc, ticks_to_wait);
}

esp_err_t uspi_get_trans_result(spi_device_handle_t handle, spi_transaction_t **trans_desc, TickType_t ticks_to_wait)
{
    return spi_device_get_trans_result(handle, trans_desc, ticks_to_wait);
}

// ============================================================================
// COMMAND/ADDRESS/DATA OPERATIONS
// ============================================================================

esp_err_t uspi_write_cmd(spi_device_handle_t handle, uint8_t cmd)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .cmd = cmd,
        .length = 0};

    return spi_device_transmit(handle, &trans);
}

esp_err_t uspi_write_cmd_data(spi_device_handle_t handle, uint8_t cmd,
                              const uint8_t *data, size_t len)
{
    if (!handle || !data || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .cmd = cmd,
        .length = len * 8,
        .tx_buffer = data};

    return spi_device_transmit(handle, &trans);
}

esp_err_t uspi_write_cmd_addr_data(spi_device_handle_t handle, uint8_t cmd,
                                   uint32_t addr, const uint8_t *data, size_t len)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .cmd = cmd,
        .addr = addr,
        .length = len * 8,
        .tx_buffer = data};

    return spi_device_transmit(handle, &trans);
}

// ============================================================================
// DMA OPERATIONS
// ============================================================================

esp_err_t uspi_transmit_dma(spi_device_handle_t handle, const uint8_t *tx_buffer,
                            size_t length, uint32_t timeout_ms)
{
    if (!handle || !tx_buffer || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .length = length * 8,
        .tx_buffer = tx_buffer,
        .rx_buffer = NULL};

    return spi_device_transmit(handle, &trans);
}

esp_err_t uspi_transfer_dma(spi_device_handle_t handle, const uint8_t *tx_buffer,
                            uint8_t *rx_buffer, size_t length, uint32_t timeout_ms)
{
    if (!handle || !tx_buffer || !rx_buffer || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .length = length * 8,
        .rxlength = length * 8,
        .tx_buffer = tx_buffer,
        .rx_buffer = rx_buffer};

    return spi_device_transmit(handle, &trans);
}

// ============================================================================
// POLLING OPERATIONS
// ============================================================================

esp_err_t uspi_polling_transmit(spi_device_handle_t handle, const uint8_t *tx_buffer,
                                size_t length)
{
    if (!handle || !tx_buffer || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = length * 8,
        .tx_buffer = tx_buffer};

    return spi_device_polling_transmit(handle, &trans);
}

esp_err_t uspi_polling_transfer(spi_device_handle_t handle, const uint8_t *tx_buffer,
                                uint8_t *rx_buffer, size_t length)
{
    if (!handle || !tx_buffer || !rx_buffer || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .length = length * 8,
        .rxlength = length * 8,
        .tx_buffer = tx_buffer,
        .rx_buffer = rx_buffer};

    return spi_device_polling_transmit(handle, &trans);
}

// ============================================================================
// INFORMATION FUNCTIONS
// ============================================================================

esp_err_t uspi_get_device_info(spi_device_handle_t handle, uspi_device_config_t *info)
{
    if (!handle || !info)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (int host = 0; host < SOC_SPI_PERIPH_NUM; host++)
    {
        if (!uspi_buses[host].is_initialized)
            continue;

        uflake_mutex_lock(uspi_buses[host].mutex, UINT32_MAX);

        spi_device_node_t *node = find_device_node(&uspi_buses[host], handle);
        if (node)
        {
            *info = node->config;
            uflake_mutex_unlock(uspi_buses[host].mutex);
            return ESP_OK;
        }

        uflake_mutex_unlock(uspi_buses[host].mutex);
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t uspi_get_device_count(spi_host_device_t host, uint8_t *count)
{
    if (host >= SOC_SPI_PERIPH_NUM || !count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!uspi_buses[host].is_initialized)
    {
        *count = 0;
        return ESP_ERR_INVALID_STATE;
    }

    *count = uspi_buses[host].device_count;
    return ESP_OK;
}
