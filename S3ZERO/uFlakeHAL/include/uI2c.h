#if !defined(UI2C_H_)
#define UI2C_H_

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "kernel.h"

// frequency constants
#define UI2C_100KHZ_FREQ_HZ 100000
#define UI2C_200KHZ_FREQ_HZ 200000
#define UI2C_400KHZ_FREQ_HZ 400000
#define UI2C_DEFAULT_FREQ_HZ UI2C_100KHZ_FREQ_HZ

#define UI2C_PORT_0 I2C_NUM_0
#define UI2C_PORT_1 I2C_NUM_1

#ifdef __cplusplus
extern "C"
{
#endif

    // Change return type to uflake_result_t for consistency
    uflake_result_t i2c_bus_manager_init(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz);

    // Deinitialize I2C bus
    esp_err_t i2c_bus_manager_deinit(i2c_port_t port);

    // Add device
    esp_err_t i2c_bus_manager_add_device(i2c_port_t port, uint8_t device_address);

    // Remove device
    esp_err_t i2c_bus_manager_remove_device(i2c_port_t port, uint8_t device_address);

    // Scan I2C bus for devices
    esp_err_t i2c_bus_manager_scan(i2c_port_t port, uint8_t *found_devices, size_t max_devices, size_t *found_count);

    // Basic I2C operations
    esp_err_t i2c_manager_write(i2c_port_t port, uint8_t device_addr, const uint8_t *write_buffer, size_t write_size);
    esp_err_t i2c_manager_read(i2c_port_t port, uint8_t device_addr, uint8_t *read_buffer, size_t read_size);
    esp_err_t i2c_manager_write_read(i2c_port_t port, uint8_t device_addr,
                                     const uint8_t *write_buffer, size_t write_size,
                                     uint8_t *read_buffer, size_t read_size);

    // Register operations
    esp_err_t i2c_manager_write_reg(i2c_port_t port, uint8_t device_addr, uint8_t reg_addr, uint8_t data);
    esp_err_t i2c_manager_read_reg(i2c_port_t port, uint8_t device_addr, uint8_t reg_addr, uint8_t *data);
    esp_err_t i2c_manager_write_reg16(i2c_port_t port, uint8_t device_addr, uint16_t reg_addr, uint8_t data);
    esp_err_t i2c_manager_read_reg16(i2c_port_t port, uint8_t device_addr, uint16_t reg_addr, uint8_t *data);

    // Multi-byte register operations
    esp_err_t i2c_manager_write_reg_bytes(i2c_port_t port, uint8_t device_addr, uint8_t reg_addr,
                                          const uint8_t *data, size_t data_len);
    esp_err_t i2c_manager_read_reg_bytes(i2c_port_t port, uint8_t device_addr, uint8_t reg_addr,
                                         uint8_t *data, size_t data_len);
#ifdef __cplusplus
}
#endif

#endif // UI2C_H_
