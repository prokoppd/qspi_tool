#ifndef QSPI_H
#define QSPI_H


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


#endif // QSPI_H

