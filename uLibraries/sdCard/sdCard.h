#if !defined(SD_CARD_H)
#define SD_CARD_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define MAX_CHAR_SIZE 64
#define SD_DETECT_PIN_DISABLED (-1)

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct
    {
        uint8_t csPin;
        int8_t sdDetectPin;
        uint32_t clockSpeedHz;
        spi_host_device_t host;
        spi_device_handle_t spi;
    } SD_CardConfig;

    int sdCard_init(SD_CardConfig *config);
    void sdCard_deinit(void);
    int sdCard_setupDetectInterrupt(SD_CardConfig *config);
    void sdCard_removeDetectInterrupt(void);

    int sdCard_readBlock(uint32_t blockNumber, uint8_t *buffer);
    int sdCard_writeBlock(uint32_t blockNumber, const uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_H
