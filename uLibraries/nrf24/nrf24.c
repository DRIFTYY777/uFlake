#include "nrf24.h"
#include "nrf24_regs.h"

#include <string.h>

#include "kernel.h"
#include "uSPI.h"

static const char *TAG = "NRF24";

char rf24_datarates[][8] = {"1Mbps", "2Mbps", "250Kbps"};
const char rf24_crclength[][10] = {"Disabled", "8 bits", "16 bits"};
char rf24_pa_dbm[][8] = {"PA_MIN", "PA_LOW", "PA_HIGH", "PA_MAX"};

// loacl copy of structure
NRF24_t *_nrf24_dev = NULL;

bool Nrf24_init(NRF24_t *dev)
{
    uspi_device_config_t dev_cfg = {
        .cs_pin = dev->csnPin,
        .clock_speed_hz = dev->frequency,
        .mode = USPI_MODE_0,
        .queue_size = 1,
        .cs_ena_pretrans = true,
        .cs_ena_posttrans = true,
        .address_bits = 0,
        .command_bits = 0,
        .dummy_bits = 0,
        .device_type = USPI_DEVICE_RADIO,
        .device_name = "NRF24",
    };

    spi_device_handle_t spi_handle;
    esp_err_t ret = uspi_device_add(dev->spiHost, &dev_cfg, &spi_handle);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return false;
    }

    UFLAKE_LOGI(TAG, "CONFIG_CE_GPIO=%d", dev->cePin);
    UFLAKE_LOGI(TAG, "CONFIG_CSN_GPIO=%d", dev->csnPin);

    // gpio_pad_select_gpio(CONFIG_CE_GPIO);
    gpio_reset_pin(dev->cePin);
    gpio_set_direction(dev->cePin, GPIO_MODE_OUTPUT);
    gpio_set_level(dev->cePin, 0);

    // gpio_pad_select_gpio(CONFIG_CSN_GPIO);
    gpio_reset_pin(dev->csnPin);
    gpio_set_direction(dev->csnPin, GPIO_MODE_OUTPUT);
    gpio_set_level(dev->csnPin, 1);

    // Store device handle and structure
    dev->spi = spi_handle;
    _nrf24_dev = dev;

    UFLAKE_LOGI(TAG, "NRF24 initialized");

    return true;
}

void Nrf24_deinit()
{
    // remove SPI device
    if (_nrf24_dev && _nrf24_dev->spi)
    {
        uspi_device_remove(_nrf24_dev->spi);
        _nrf24_dev = NULL;
    }
}

bool spi_write_byte(uint8_t *Dataout, size_t DataLength)
{
    if (!_nrf24_dev || !_nrf24_dev->spi)
    {
        UFLAKE_LOGE(TAG, "NRF24 device not initialized");
        return false;
    }

    esp_err_t ret = uspi_transmit(_nrf24_dev->spi, Dataout, DataLength, 100);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "SPI write failed: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}

bool spi_read_byte(uint8_t *Datain, uint8_t *Dataout, size_t DataLength)
{
    if (!_nrf24_dev || !_nrf24_dev->spi)
    {
        UFLAKE_LOGE(TAG, "NRF24 device not initialized");
        return false;
    }

    esp_err_t ret = uspi_transfer(_nrf24_dev->spi, Dataout, Datain, DataLength, 100);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "SPI read failed: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}

uint8_t spi_transfer(uint8_t address)
{
    if (!_nrf24_dev || !_nrf24_dev->spi)
    {
        UFLAKE_LOGE(TAG, "NRF24 device not initialized");
        return 0;
    }

    uint8_t received = 0;
    esp_err_t ret = uspi_transfer(_nrf24_dev->spi, &address, &received, 1, 100);
    if (ret != ESP_OK)
    {
        UFLAKE_LOGE(TAG, "SPI transfer failed: %s", esp_err_to_name(ret));
        return 0;
    }

    return received;
}

void spi_csnLow()
{
    if (_nrf24_dev)
    {
        gpio_set_level(_nrf24_dev->csnPin, 0);
    }
}

void spi_csnHi()
{
    if (_nrf24_dev)
    {
        gpio_set_level(_nrf24_dev->csnPin, 1);
    }
}

bool Nrf24_isConnected(NRF24_t *dev)
{
    uint8_t test_addr[5] = {0xA5, 0xA5, 0xA5, 0xA5, 0xA5};
    uint8_t read_addr[5] = {0};

    // Write test address to TX_ADDR register
    spi_csnLow();
    uint8_t write_cmd = W_REGISTER | TX_ADDR;
    spi_transfer(write_cmd);
    for (size_t i = 0; i < 5; i++)
    {
        spi_transfer(test_addr[i]);
    }
    spi_csnHi();

    // Read back the TX_ADDR register
    spi_csnLow();
    uint8_t read_cmd = R_REGISTER | TX_ADDR;
    spi_transfer(read_cmd);
    for (size_t i = 0; i < 5; i++)
    {
        read_addr[i] = spi_transfer(NOP);
    }
    spi_csnHi();

    // Compare written and read addresses
    if (memcmp(test_addr, read_addr, 5) == 0)
    {
        UFLAKE_LOGI(TAG, "NRF24 is connected");
        return true;
    }
    else
    {
        UFLAKE_LOGE(TAG, "NRF24 is NOT connected");
        return false;
    }
}
