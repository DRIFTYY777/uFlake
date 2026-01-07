#if !defined(SD_CARD_H)
#define SD_CARD_H

#include <stdint.h>
#include "driver/spi_master.h"

#define MAX_CHAR_SIZE 64

#ifdef __cplusplus
extern "C"
{
#endif
    typedef struct
    {
        uint8_t csPin;
        uint32_t clockSpeedHz;
        spi_host_device_t host;
        spi_device_handle_t spi;
    } SD_CardConfig;

    int sdCard_init(SD_CardConfig *config);
    int sdCard_readBlock(uint32_t blockNumber, uint8_t *buffer);
    int sdCard_writeBlock(uint32_t blockNumber, const uint8_t *buffer);
    void sdCard_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // SD_CARD_H
