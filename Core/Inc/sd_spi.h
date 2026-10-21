/*
 * sd_spi.h
 *
 *  Created on: Jun 27, 2026
 *      Author: hasmi
 */

#ifndef INC_SD_SPI_H_
#define INC_SD_SPI_H_

#include "main.h"
#include <stdint.h>

void SD_SPI_Init(void);
void SD_SPI_Select(void);
void SD_SPI_Unselect(void);
uint8_t SD_SPI_TxRx(uint8_t data);
void SD_SPI_SendDummyClocks(void);
HAL_StatusTypeDef SD_SPI_Transmit(
    const uint8_t *data,
    uint32_t length
);

HAL_StatusTypeDef SD_SPI_TransmitBuffer(
    const uint8_t *buffer,
    uint16_t length
);


#endif /* INC_SD_SPI_H_ */
