#include <stdio.h>
#include <stdlib.h>

#include "qspi.h"
#include "slog.h"
#include <assert.h>

#include <unistd.h>

#include "internals.h"

#define VERSION "0.1.3"

// print binray buffer with slog

int main()
{
    assert(QSPI_IsInitialized() == 0 && "QSPI should not be initialized at the start");
    slog_init("qspi_tool", SLOG_FLAGS_ALL, 0);
    slog_info("Starting QSPI tool v%.*s...", lengthof(VERSION), VERSION);
    slog_info("Initializing QSPI...");
    slog_info("PAGE_SIZE: %zu", sysconf(_SC_PAGE_SIZE));

    QSPI_Init();

    if (QSPI_IsInitialized())
    {
        slogi("QSPI initialized successfully");
    }
    else
    {
        QSPI_DeInit();
        slogf("QSPI initialization failed");
        slog_destroy();
        return EXIT_FAILURE;
    }

    QSPI_DeInit();
    slog_destroy();
    return EXIT_SUCCESS;
}
