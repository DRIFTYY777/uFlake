#include "uI2c.h"
#include <string.h>

static const char *TAG = "UI2C";

// I2C bus configuration structure (updated for new API)
typedef struct i2c_bus_config
{
    i2c_port_t port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t freq_hz;
    bool is_initialized;
    uflake_mutex_t *mutex;
    i2c_master_bus_handle_t bus_handle;
    struct i2c_device_node *device_list;
} i2c_bus_config_t;

// I2C device node for linked list (updated for new API)
typedef struct i2c_device_node
{
    uint8_t device_address;
    uint32_t ref_count;
    i2c_master_dev_handle_t dev_handle;
    struct i2c_device_node *next;
} i2c_device_node_t;

// Global I2C bus configurations (ESP32-S3 has 2 I2C ports)
static i2c_bus_config_t u_i2c_buses[I2C_NUM_MAX] = {0};

// ============================================================================
// PRIVATE HELPER FUNCTIONS
// ============================================================================

static i2c_device_node_t *find_device(i2c_bus_config_t *bus, uint8_t device_addr)
{
    i2c_device_node_t *current = bus->device_list;
    while (current)
    {
        if (current->device_address == device_addr)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static esp_err_t add_device_to_list(i2c_bus_config_t *bus, uint8_t device_addr)
{
    // Check if device already exists
    i2c_device_node_t *existing = find_device(bus, device_addr);
    if (existing)
    {
        existing->ref_count++;
        UFLAKE_LOGD(TAG, "Device 0x%02X already registered, ref_count: %d",
                    device_addr, (int)existing->ref_count);
        return ESP_OK;
    }

    // Allocate new device node
    i2c_device_node_t *node = (i2c_device_node_t *)uflake_malloc(
        sizeof(i2c_device_node_t), UFLAKE_MEM_INTERNAL);

    if (!node)
    {
        UFLAKE_LOGE(TAG, "Failed to allocate memory for device node");
        return ESP_ERR_NO_MEM;
    }

    // New API: Create device handle
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_addr,
        .scl_speed_hz = bus->freq_hz,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus->bus_handle, &dev_cfg, &node->dev_handle);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to add device 0x%02X: %s", device_addr, esp_err_to_name(ret));
        uflake_free(node);
        return ret;
    }

    node->device_address = device_addr;
    node->ref_count = 1;
    node->next = bus->device_list;
    bus->device_list = node;

    UFLAKE_LOGI(TAG, "Added device 0x%02X to bus %d", device_addr, bus->port);
    return ESP_OK;
}

static esp_err_t remove_device_from_list(i2c_bus_config_t *bus, uint8_t device_addr)
{
    i2c_device_node_t *prev = NULL;
    i2c_device_node_t *current = bus->device_list;

    while (current)
    {
        if (current->device_address == device_addr)
        {
            current->ref_count--;

            if (current->ref_count == 0)
            {
                //  New API: Remove device handle
                i2c_master_bus_rm_device(current->dev_handle);

                // Remove from list
                if (prev)
                {
                    prev->next = current->next;
                }
                else
                {
                    bus->device_list = current->next;
                }

                uflake_free(current);
                UFLAKE_LOGI(TAG, "Removed device 0x%02X from bus %d", device_addr, bus->port);
            }
            else
            {
                UFLAKE_LOGD(TAG, "Device 0x%02X ref_count: %d", device_addr, (int)current->ref_count);
            }

            return ESP_OK;
        }
        prev = current;
        current = current->next;
    }

    return ESP_ERR_NOT_FOUND;
}

uflake_result_t i2c_bus_manager_init(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz)
{
    if (port >= I2C_NUM_MAX)
    {
        UFLAKE_LOGE(TAG, "Invalid I2C port: %d", port);
        return UFLAKE_ERROR_INVALID_PARAM;
    }

    if (u_i2c_buses[port].is_initialized)
    {
        UFLAKE_LOGW(TAG, "I2C port %d already initialized", port);
        return UFLAKE_OK;
    }

    // Create mutex for this bus
    if (uflake_mutex_create(&u_i2c_buses[port].mutex) != UFLAKE_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to create mutex for I2C port %d", port);
        return UFLAKE_ERROR_MEMORY;
    }

    //  New API: Configure master bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = port,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0, //  Auto-select priority
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    //  New API: Create bus
    esp_err_t err = i2c_new_master_bus(&bus_config, &u_i2c_buses[port].bus_handle);
    if (err != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        uflake_mutex_destroy(u_i2c_buses[port].mutex);
        return UFLAKE_ERROR;
    }

    // Save configuration
    u_i2c_buses[port].port = port;
    u_i2c_buses[port].sda_pin = sda_pin;
    u_i2c_buses[port].scl_pin = scl_pin;
    u_i2c_buses[port].freq_hz = freq_hz;
    u_i2c_buses[port].is_initialized = true;
    u_i2c_buses[port].device_list = NULL;

    UFLAKE_LOGI(TAG, "I2C bus %d initialized: SDA=%d, SCL=%d, Freq=%d Hz",
                port, sda_pin, scl_pin, (int)freq_hz);

    return UFLAKE_OK;
}

esp_err_t i2c_bus_manager_deinit(i2c_port_t port)
{
    if (port >= I2C_NUM_MAX || !u_i2c_buses[port].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uflake_mutex_lock(u_i2c_buses[port].mutex, UINT32_MAX);

    // Free all device nodes
    i2c_device_node_t *current = u_i2c_buses[port].device_list;
    while (current)
    {
        i2c_device_node_t *next = current->next;
        i2c_master_bus_rm_device(current->dev_handle); //  New API
        uflake_free(current);
        current = next;
    }

    //  New API: Delete bus
    esp_err_t err = i2c_del_master_bus(u_i2c_buses[port].bus_handle);

    uflake_mutex_unlock(u_i2c_buses[port].mutex);
    uflake_mutex_destroy(u_i2c_buses[port].mutex);

    u_i2c_buses[port].is_initialized = false;
    u_i2c_buses[port].device_list = NULL;

    UFLAKE_LOGI(TAG, "I2C bus %d deinitialized", port);
    return err;
}

esp_err_t i2c_bus_manager_add_device(i2c_port_t port, uint8_t device_address)
{
    if (port >= I2C_NUM_MAX || !u_i2c_buses[port].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (device_address > 0x7F)
    {
        UFLAKE_LOGE(TAG, "Invalid I2C address: 0x%02X", device_address);
        return ESP_ERR_INVALID_ARG;
    }

    uflake_mutex_lock(u_i2c_buses[port].mutex, UINT32_MAX);
    esp_err_t result = add_device_to_list(&u_i2c_buses[port], device_address);
    uflake_mutex_unlock(u_i2c_buses[port].mutex);

    return result;
}

esp_err_t i2c_bus_manager_remove_device(i2c_port_t port, uint8_t device_address)
{
    if (port >= I2C_NUM_MAX || !u_i2c_buses[port].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uflake_mutex_lock(u_i2c_buses[port].mutex, UINT32_MAX);
    esp_err_t result = remove_device_from_list(&u_i2c_buses[port], device_address);
    uflake_mutex_unlock(u_i2c_buses[port].mutex);

    return result;
}

esp_err_t i2c_bus_manager_scan(i2c_port_t port, uint8_t *found_devices,
                               size_t max_devices, size_t *found_count)
{
    if (port >= I2C_NUM_MAX || !u_i2c_buses[port].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!found_devices || !found_count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uflake_mutex_lock(u_i2c_buses[port].mutex, UINT32_MAX);

    *found_count = 0;
    UFLAKE_LOGI(TAG, "Scanning I2C bus %d...", port);

    // New API Use i2c_master_probe
    for (uint16_t addr = 0x08; addr < 0x78; addr++)
    {
        esp_err_t ret = i2c_master_probe(u_i2c_buses[port].bus_handle, addr, 50);

        if (ret == ESP_OK)
        {
            if (*found_count < max_devices)
            {
                found_devices[*found_count] = (uint8_t)addr;
                (*found_count)++;
                UFLAKE_LOGI(TAG, "Found device at 0x%02X", addr);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    uflake_mutex_unlock(u_i2c_buses[port].mutex);

    UFLAKE_LOGI(TAG, "Scan complete: found %d devices", (int)*found_count);
    return ESP_OK;
}

// ============================================================================
// I2C READ/WRITE OPERATIONS (New API)
// ============================================================================

esp_err_t i2c_manager_write(i2c_port_t port, uint8_t device_addr,
                            const uint8_t *write_buffer, size_t write_size)
{
    if (port >= I2C_NUM_MAX || !u_i2c_buses[port].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!write_buffer || write_size == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uflake_mutex_lock(u_i2c_buses[port].mutex, UINT32_MAX);

    // Find device handle
    i2c_device_node_t *dev_node = find_device(&u_i2c_buses[port], device_addr);
    if (!dev_node)
    {
        uflake_mutex_unlock(u_i2c_buses[port].mutex);
        UFLAKE_LOGE(TAG, "Device 0x%02X not registered", device_addr);
        return ESP_ERR_NOT_FOUND;
    }

    // New API i2c_master_transmit
    esp_err_t ret = i2c_master_transmit(dev_node->dev_handle, write_buffer, write_size, 1000);

    uflake_mutex_unlock(u_i2c_buses[port].mutex);

    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t i2c_manager_read(i2c_port_t port, uint8_t device_addr,
                           uint8_t *read_buffer, size_t read_size)
{
    if (port >= I2C_NUM_MAX || !u_i2c_buses[port].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!read_buffer || read_size == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uflake_mutex_lock(u_i2c_buses[port].mutex, UINT32_MAX);

    i2c_device_node_t *dev_node = find_device(&u_i2c_buses[port], device_addr);
    if (!dev_node)
    {
        uflake_mutex_unlock(u_i2c_buses[port].mutex);
        return ESP_ERR_NOT_FOUND;
    }

    // New API i2c_master_receive
    esp_err_t ret = i2c_master_receive(dev_node->dev_handle, read_buffer, read_size, 1000);

    uflake_mutex_unlock(u_i2c_buses[port].mutex);

    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t i2c_manager_write_read(i2c_port_t port, uint8_t device_addr,
                                 const uint8_t *write_buffer, size_t write_size,
                                 uint8_t *read_buffer, size_t read_size)
{
    if (port >= I2C_NUM_MAX || !u_i2c_buses[port].is_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (!write_buffer || write_size == 0 || !read_buffer || read_size == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uflake_mutex_lock(u_i2c_buses[port].mutex, UINT32_MAX);

    i2c_device_node_t *dev_node = find_device(&u_i2c_buses[port], device_addr);
    if (!dev_node)
    {
        uflake_mutex_unlock(u_i2c_buses[port].mutex);
        return ESP_ERR_NOT_FOUND;
    }

    // New API i2c_master_transmit_receive
    esp_err_t ret = i2c_master_transmit_receive(dev_node->dev_handle,
                                                write_buffer, write_size,
                                                read_buffer, read_size, 1000);

    uflake_mutex_unlock(u_i2c_buses[port].mutex);

    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "I2C write-read failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

// ============================================================================
// REGISTER OPERATIONS
// ============================================================================

esp_err_t i2c_manager_write_reg(i2c_port_t port, uint8_t device_addr,
                                uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_manager_write(port, device_addr, write_buf, 2);
}

esp_err_t i2c_manager_read_reg(i2c_port_t port, uint8_t device_addr,
                               uint8_t reg_addr, uint8_t *data)
{
    return i2c_manager_write_read(port, device_addr, &reg_addr, 1, data, 1);
}

esp_err_t i2c_manager_write_reg16(i2c_port_t port, uint8_t device_addr,
                                  uint16_t reg_addr, uint8_t data)
{
    uint8_t write_buf[3] = {
        (uint8_t)(reg_addr >> 8),
        (uint8_t)(reg_addr & 0xFF),
        data};
    return i2c_manager_write(port, device_addr, write_buf, 3);
}

esp_err_t i2c_manager_read_reg16(i2c_port_t port, uint8_t device_addr,
                                 uint16_t reg_addr, uint8_t *data)
{
    uint8_t write_buf[2] = {
        (uint8_t)(reg_addr >> 8),
        (uint8_t)(reg_addr & 0xFF)};
    return i2c_manager_write_read(port, device_addr, write_buf, 2, data, 1);
}

esp_err_t i2c_manager_write_reg_bytes(i2c_port_t port, uint8_t device_addr,
                                      uint8_t reg_addr, const uint8_t *data,
                                      size_t data_len)
{
    if (!data || data_len == 0 || data_len > 255)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *write_buf = (uint8_t *)uflake_malloc(data_len + 1, UFLAKE_MEM_INTERNAL);
    if (!write_buf)
    {
        return ESP_ERR_NO_MEM;
    }

    write_buf[0] = reg_addr;
    memcpy(write_buf + 1, data, data_len);

    esp_err_t ret = i2c_manager_write(port, device_addr, write_buf, data_len + 1);

    uflake_free(write_buf);
    return ret;
}

esp_err_t i2c_manager_read_reg_bytes(i2c_port_t port, uint8_t device_addr,
                                     uint8_t reg_addr, uint8_t *data,
                                     size_t data_len)
{
    return i2c_manager_write_read(port, device_addr, &reg_addr, 1, data, data_len);
}
