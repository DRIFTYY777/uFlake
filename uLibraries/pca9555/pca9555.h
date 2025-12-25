#if !defined(PCA95555)
#define PCA95555

#include <stdint.h>
#include "driver/i2c.h"

#define PCA9555_ADDRESS 0x20 // Default I2C address for PCA9555

// PCA9555 Register Addresses
#define PCA9555_INPUT_PORT_0 0x00
#define PCA9555_INPUT_PORT_1 0x01
#define PCA9555_OUTPUT_PORT_0 0x02
#define PCA9555_OUTPUT_PORT_1 0x03
#define PCA9555_POLARITY_INV_0 0x04
#define PCA9555_POLARITY_INV_1 0x05
#define PCA9555_CONFIG_0 0x06
#define PCA9555_CONFIG_1 0x07

#define PCA9555_NORMAL_MODE 0x00
#define PCA9555_INVERTED_MODE 0xFF

#define PCA9555_DiRECTION_INPUT 0x00
#define PCA9555_DiRECTION_OUTPUT 0xFF

#ifdef __cplusplus
extern "C"
{
#endif
    void init_pca9555_as_input(i2c_port_t port, uint8_t device_address);
    uint16_t read_pca9555_inputs(i2c_port_t port, uint8_t device_address);

#ifdef __cplusplus
}
#endif

#endif // PCA95555
