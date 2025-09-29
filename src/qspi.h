#ifndef QSPI_H
#define QSPI_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initializes the QSPI (Quad SPI) interface.
 */
void QSPI_Init();

/**
 * @brief Checks if the QSPI interface is initialized.
 *
 * @return 1 if initialized, 0 otherwise.
 */
int QSPI_IsInitialized();

/**
 * @brief De-initializes the QSPI interface and cleans up resources.
 */
void QSPI_DeInit();

int QSPI_Busy(void);

void QSPI_SetupLut(uint32_t *lut, size_t len);

int QSPI_Write(uint32_t addr, uint8_t lut_index,  uint8_t *buffer, size_t size);

int QSPI_Read(uint32_t addr, uint8_t *buffer, size_t size);

int QSPI_ReadSample(uint32_t addr, void *sample, size_t size);

#endif // QSPI_H
