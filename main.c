#include <stdio.h>
#include <stdlib.h>

#include "slog.h"
#include "qspi.h"
#include <assert.h>

int main()
{
    assert(QSPI_IsInitialized() == 0 && "QSPI should not be initialized at the start");
    slog_init("qspi_tool", SLOG_FLAGS_ALL, 0);
    slog_info("Starting QSPI tool...");
    slog_info("Initializing QSPI...");
    QSPI_Init();
    
    if (QSPI_IsInitialized())
    {
        slogi("QSPI initialized successfully");
    }
    else
    {
        slogf("QSPI initialization failed");
        slog_destroy();
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
