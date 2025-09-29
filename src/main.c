#include <stdio.h>
#include <stdlib.h>

#include "qspi.h"
#include "slog.h"
#include <assert.h>

#include <string.h>
#include <unistd.h>

#include "fpga_interface.h"

#define VERSION "0.1.5"

static int parse_flags(int argc, char *argv[])
{
    uint16_t flags = SLOG_FLAGS_ALL;

    if (argc == 1)
    {
        return flags;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-nl") == 0)
        {
            return 0;
        }
        else if (strcmp(argv[i], "--no-debug") == 0)
        {
            flags &= ~SLOG_DEBUG;
        }
        else if (strcmp(argv[i], "--no-info") == 0)
        {
            flags &= ~SLOG_INFO;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --no-debug       Disable debug logging\n");
            printf("  --no-info        Disable info logging\n");
            printf("  -nl              Disable all logging\n");
            printf("  -h, --help      Show this help message\n");
            exit(EXIT_SUCCESS);
        }
    }
    return flags;
}

const uint32_t test_lut[] = {
    [0] = FLEXSPI_LUT_SEQ(LUT_CMD, kFlexSPI_4PAD, 0x8F, LUT_READ, kFlexSPI_4PAD, LUT_DUMMY),
    [1] = 0,
    [2] = 0,
    [3] = 0,
};

int main(int argc, char *argv[])
{
    uint16_t nFlags = parse_flags(argc, argv);

    assert(QSPI_IsInitialized() == 0 && "QSPI should not be initialized at the start");
    slog_init("qspi_tool", nFlags, 0);
    slog_info("Starting QSPI tool v%.*s...", lengthof(VERSION), VERSION);

    QSPI_Init();
    // QSPI_SetupLut((uint32_t *)fpga_lut, sizeof(fpga_lut));
    QSPI_SetupLut((uint32_t *)test_lut, sizeof(test_lut));
    if (!QSPI_IsInitialized())
    {
        QSPI_DeInit();
        slogf("QSPI initialization failed");
        slog_destroy();
        return EXIT_FAILURE;
    }
    slogi("QSPI initialized successfully");

    if(QSPI_Write(0xCAFECAFE, 0,NULL , 0) != 0) {
        slogf("QSPI write failed");
    }

    QSPI_DeInit();
    slog_destroy();
    return EXIT_SUCCESS;
}
