#ifndef QSPI_H
#define QSPI_H




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



#endif // QSPI_H

