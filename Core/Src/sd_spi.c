#include "sd_spi.h"

extern SPI_HandleTypeDef hspi2;

void SD_SPI_Init(void)
{
    SD_SPI_Unselect();
    SD_SPI_SendDummyClocks();
}

void SD_SPI_Select(void)
{
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_RESET
    );

    /* Extra clock after selecting the card. */
    SD_SPI_TxRx(0xFFU);
}

void SD_SPI_Unselect(void)
{
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    /* Extra clock after releasing the card. */
    SD_SPI_TxRx(0xFFU);
}

uint8_t SD_SPI_TxRx(uint8_t data)
{
    uint8_t rx = 0xFFU;

    if (HAL_SPI_TransmitReceive(
            &hspi2,
            &data,
            &rx,
            1U,
            HAL_MAX_DELAY
        ) != HAL_OK)
    {
        return 0xFFU;
    }

    return rx;
}

HAL_StatusTypeDef SD_SPI_TransmitBuffer(
    const uint8_t *buffer,
    uint16_t length
)
{
    if (buffer == NULL || length == 0U)
    {
        return HAL_ERROR;
    }

    return HAL_SPI_Transmit(
        &hspi2,
        (uint8_t *)buffer,
        length,
        HAL_MAX_DELAY
    );
}

void SD_SPI_SendDummyClocks(void)
{
    /*
     * SD card requires at least 74 clock pulses
     * with CS high during initialization.
     */
    HAL_GPIO_WritePin(
        SD_CS_GPIO_Port,
        SD_CS_Pin,
        GPIO_PIN_SET
    );

    for (uint8_t i = 0U; i < 10U; i++)
    {
        SD_SPI_TxRx(0xFFU);
    }
}
