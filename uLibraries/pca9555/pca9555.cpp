#include "pca9555.h"
#include "uI2c.h"

// initialize PCA9555 as buttons input
void init_pca9555_as_input(i2c_port_t port, uint8_t device_address)
{
    // add device to I2C bus
    i2c_bus_manager_add_device(port, device_address);
    // Set all pins as input
    i2c_manager_write_reg_bytes(port, device_address, PCA9555_CONFIG_0, (uint8_t[]){PCA9555_DiRECTION_INPUT, PCA9555_DiRECTION_INPUT}, 2);
    // Set polarity inversion if needed
    i2c_manager_write_reg_bytes(port, device_address, PCA9555_POLARITY_INV_0, (uint8_t[]){PCA9555_NORMAL_MODE, PCA9555_NORMAL_MODE}, 2);
}

uint16_t read_pca9555_inputs(i2c_port_t port, uint8_t device_address)
{
    uint8_t input_data[2] = {0};
    i2c_manager_read_reg_bytes(port, device_address, PCA9555_INPUT_PORT_0, input_data, 2);
    return (input_data[1] << 8) | input_data[0];
}