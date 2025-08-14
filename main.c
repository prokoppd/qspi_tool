#include <stdio.h>
#include <stdlib.h>

#include "slog.h"
#include "qspi.h"

int main()
{
    slog_init("qspi_tool", SLOG_FLAGS_ALL, 0);
    slog_info("Starting QSPI tool...");
    slog_info("Initializing QSPI...");
    QSPI_Init();
    return EXIT_SUCCESS;
}
