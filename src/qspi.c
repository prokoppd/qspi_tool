#include "qspi.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "flexspi.h"
#include "fpga_interface.h"
#include "utils.h"

#include "slog.h"

#define PAGE_SIZE_64K        (64 * 1024)
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

#define UINT64(VALUE)     ((uint64_t)(VALUE))
#define UINT64_PTR(PTR)   ((uint64_t *)(PTR))
#define PTR_U64VALUE(PTR) *(UINT64_PTR(PTR))

#define UINT32(VALUE)     ((uint32_t)(VALUE))
#define UINT32_PTR(PTR)   ((uint32_t *)(PTR))
#define PTR_U32VALUE(PTR) *(UINT32_PTR(PTR))

#define LOG_REGISTER(VIRT, PHY) slogt("0x%0lX (0x%0lX): 0x%08X", PHY, VIRT, PTR_U32VALUE(VIRT));

#define REG(BASE, OFFSET)      (void *)(UINT64(BASE) + UINT64(OFFSET))
#define VIRT_REG(VIRT, OFFSET) REG((VIRT), (OFFSET))
#define CCM_REG(OFFSET)        REG(CCM_BASE, (OFFSET))

#define IOMUXC_BASE                   0x3033'0000
#define IOMUXC_OFFSET_FLEXSPI_A_SCLK  0xE0
#define IOMUXC_OFFSET_FLEXSPI_A_SS0_B 0xE4
#define IOMUXC_OFFSET_FLEXSPI_A_DATA0 0xF8
#define IOMUXC_OFFSET_FLEXSPI_A_DATA1 0xFC
#define IOMUXC_OFFSET_FLEXSPI_A_DATA2 0x100
#define IOMUXC_OFFSET_FLEXSPI_A_DATA3 0x104

#define IOMUXC_ALT1 1
#define IOMUXC_SION 0x10

typedef struct QSPI_Context
{
    int           fd;
    int           page_size;
    int           init_done;
    int           lut_seted;
    FlexSPI_Type *flexspi;
    void         *map_fspi;
    void         *map_ccm;
    void         *map_rdc;
    void         *map_iomux;

} QSPI_Context;

typedef enum CommandSeequence
{
    CMD_WRITE = 0,
} CommandSeequence;

static const uint32_t pre_div  = 0;   // Pre-divider value (1-8)
static const uint32_t post_div = 0x7; // Post-divider value (1-64
static const uint32_t clk_mux  = 0x2; // Clock mux value (see _ccm_rootmux_xxx enumeration)

static QSPI_Context qspi_ctx;

static void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    (void)signal;
    (void)arg;
    slogf("Caught segfault at address %p", si->si_addr);
    QSPI_DeInit();
    slog_destroy();
    exit(EXIT_FAILURE);
}

static int map_memory(void **map, int fd, uint32_t base, size_t size, long int page_size)
{
    assert(map != NULL);
    assert(fd >= 0);
    assert(size > 0);
    assert(page_size > 0);

    *map = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (base & ~(page_size - 1)));
    if (*map == MAP_FAILED)
    {
        return -1;
    }
    return 0;
}

static int request_flexspi_memory(QSPI_Context *ctx)
{
    assert(ctx != NULL);
    // Map the FlexSPI control registers
    if (map_memory(&ctx->map_fspi, ctx->fd, FLEXSPI_BASE, sizeof(FlexSPI_Type), ctx->page_size) != 0)
    {
        slogf("Failed to mmap FlexSPI base: %s", strerror(errno));
        return -1;
    }
    ctx->flexspi = (FlexSPI_Type *)(UINT64(ctx->map_fspi) + UINT64(FLEXSPI_BASE & (ctx->page_size - 1)));
    return 0;
}

static void clock_init(QSPI_Context *ctx, uint32_t mux, uint32_t pre, uint32_t post)
{
    assert(ctx != NULL);
    assert(ctx->fd >= 0);
    void    *ccm_base;
    uint32_t value;

    if (map_memory(&ctx->map_ccm, ctx->fd, CCM_BASE, PAGE_SIZE_64K, ctx->page_size) != 0)
    {
        slogf("Failed to mmap CCM base: %s", strerror(errno));
        return;
    }

    ccm_base = (void *)(UINT64(ctx->map_ccm) + UINT64(CCM_BASE & (ctx->page_size - 1)));

    /* Domain clocks needed all the time */
    PTR_U32VALUE(VIRT_REG(ccm_base, CCM_CCGR47)) = 0x3;
    // PTR_U32VALUE(VIRT_REG(ccm_base, CCM_CCGR47)) = 0xC;
    LOG_REGISTER(VIRT_REG(ccm_base, CCM_CCGR47), REG(CCM_BASE, CCM_CCGR47));

    value = CLK_ROOT_EN | MUX_CLK_ROOT_SELECT(mux) | PRE_PODF(pre) | POST_PODF(post);

    PTR_U32VALUE(VIRT_REG(ccm_base, QSPI_CLK_ROOT)) = value;
    // PTR_U32VALUE(VIRT_REG(ccm_base, QSPI_CLK_ROOT)) = 0x07000002;
    PTR_U32VALUE(VIRT_REG(ccm_base, QSPI_CLK_ROOT)) |= CLK_ROOT_EN;
    LOG_REGISTER(VIRT_REG(ccm_base, QSPI_CLK_ROOT), REG(CCM_BASE, QSPI_CLK_ROOT));
}

void iomux_init(QSPI_Context *ctx)
{
    assert(ctx != NULL);
    assert(ctx->fd >= 0);
    void *iomux_base;

    if (map_memory(&ctx->map_iomux, ctx->fd, IOMUXC_BASE, ctx->page_size, ctx->page_size) != 0)
    {
        slogf("Failed to mmap IOMUXC base: %s", strerror(errno));
        return;
    }

    iomux_base = (void *)(UINT64(ctx->map_iomux) + UINT64(IOMUXC_BASE & (ctx->page_size - 1)));

    PTR_U32VALUE(VIRT_REG(iomux_base, IOMUXC_OFFSET_FLEXSPI_A_SCLK)) = IOMUXC_ALT1 | IOMUXC_SION; // FLEXSPI_A_SCLK
    PTR_U32VALUE(VIRT_REG(iomux_base, IOMUXC_OFFSET_FLEXSPI_A_SS0_B)) = IOMUXC_ALT1 | IOMUXC_SION; // FLEXSPI_A_SS0_B
    PTR_U32VALUE(VIRT_REG(iomux_base, IOMUXC_OFFSET_FLEXSPI_A_DATA0)) = IOMUXC_ALT1 | IOMUXC_SION; // FLEXSPI_A_DATA0
    PTR_U32VALUE(VIRT_REG(iomux_base, IOMUXC_OFFSET_FLEXSPI_A_DATA1)) = IOMUXC_ALT1 | IOMUXC_SION; // FLEXSPI_A_DATA1
    PTR_U32VALUE(VIRT_REG(iomux_base, IOMUXC_OFFSET_FLEXSPI_A_DATA2)) = IOMUXC_ALT1 | IOMUXC_SION; // FLEXSPI_A_DATA2
    PTR_U32VALUE(VIRT_REG(iomux_base, IOMUXC_OFFSET_FLEXSPI_A_DATA3)) = IOMUXC_ALT1 | IOMUXC_SION; // FLEXSPI_A_DATA3

}

static inline void clear_flags(FlexSPI_Type *fspi)
{
    // slogt("Clearing flags");
    fspi->INTR = fspi->INTR; // Clear IP command done interrupt
    fspi->STS0 = fspi->STS0; // Clear status flags
    fspi->STS1 = fspi->STS1; // Clear status flags
    // slogt("Flags cleared");
}
#define FLEXSPI_IPTXFCR_WTR_MASK  (0x1FC)
#define FLEXSPI_IPTXFCR_WTR_SHIFT (2U)
#define FLEXSPI_IPRXFCR_RTR_MASK  (0x1FC)
#define FLEXSPI_IPRXFCR_RTR_SHIFT (2U)

static int write_blocking(FlexSPI_Type *fspi, uint8_t *buffer, size_t size)
{
    assert(size <= (32 * 32));
    uint32_t i         = 0, j;
    uint32_t watermark = ((fspi->IPTXFCR & FLEXSPI_IPTXFCR_WTR_MASK) >> FLEXSPI_IPTXFCR_WTR_SHIFT) + 1;

    // fspi->IPTXFCR |= 1; // Flush TX FIFO

    // Wait until TX FIFO is empty
    while (0 != size)
    {
        while (0 == (fspi->INTR & (1 << 6))) // bit6 = IPTXFEMPTY
        {
        }
        clear_flags(fspi);
        // slogt("remining: %d", size);
        if (size >= 8 * watermark)
        {
            for (i = 0U; i < 2U * watermark; i++)
            {
                fspi->TFDR[i] = *(uint32_t *)(void *)buffer;
                buffer += 4U;
            }

            size = size - 8U * watermark;
        }
        else
        {
            /* Write word aligned data into tx fifo. */
            for (i = 0U; i < (size / 4U); i++)
            {
                fspi->TFDR[i] = *(uint32_t *)(void *)buffer;
                buffer += 4U;
            }

            /* Adjust size by the amount processed. */
            size -= 4U * i;

            /* Write word un-aligned data into tx fifo. */
            if (0x00U != size)
            {
                uint32_t tempVal = 0x00U;

                for (j = 0U; j < size; j++)
                {
                    tempVal |= ((uint32_t)*buffer++ << (8U * j));
                }

                fspi->TFDR[i] = tempVal;
            }

            size = 0U;
        }
        /* Push a watermark level data into IP TX FIFO. */
        fspi->INTR |= (1 << 6);
    }
    return 0;
}

static int read_blocking(FlexSPI_Type *fspi, uint8_t *buffer, size_t size)
{
    assert(size <= (32 * 32));
    uint32_t i         = 0, j;
    uint32_t watermark = ((fspi->IPRXFCR & FLEXSPI_IPRXFCR_RTR_MASK) >> FLEXSPI_IPRXFCR_RTR_SHIFT) + 1;

    fspi->IPRXFCR |= 1; // Flush RX FIFO

    // Wait until RX FIFO is not empty
    while (0 != size)
    {
        while (0 == (fspi->INTR & (1 << 7))) // bit7 = IPRXFWMF
        {
        }
        clear_flags(fspi);
        // slogt("remining: %d", size);
        if (size >= 4 * watermark)
        {
            for (i = 0U; i < watermark; i++)
            {
                *(uint32_t *)(void *)buffer = fspi->RFDR[i];
                buffer += 4U;
            }

            size = size - 4U * watermark;
        }
        else
        {
            /* Read word aligned data from rx fifo. */
            for (i = 0U; i < (size / 4U); i++)
            {
                *(uint32_t *)(void *)buffer = fspi->RFDR[i];
                buffer += 4U;
            }

            /* Adjust size by the amount processed. */
            size -= 4U * i;

            /* Read word un-aligned data from rx fifo. */
            if (0x00U != size)
            {
                uint32_t tempVal = fspi->RFDR[i];

                for (j = 0U; j < size; j++)
                {
                    *buffer++ = (uint8_t)(tempVal >> (8U * j));
                }
            }

            size = 0U;
        }
        /* Push a watermark level data into IP RX FIFO. */
        fspi->INTR |= (1 << 7);
    }
    return 0;
}

static inline void lock_lut(FlexSPI_Type *fspi)
{
    // Lock LUT after update
    fspi->LUTKEY = FSPI_LUTKEY_VALUE;
    fspi->LUTCR  = FSPI_LOCKER_LOCK;
}

static inline void unlock_lut(FlexSPI_Type *fspi)
{
    // Unlock LUT for update
    fspi->LUTKEY = FSPI_LUTKEY_VALUE;
    fspi->LUTCR  = FSPI_LOCKER_UNLOCK;
}

#define INSTR(op, pads, opr) (((op) << 10) | ((pads) << 8) | (opr))
// #define LUT0(op0, pads0, opr0, op1, pads1, opr1) ((op0) | ((pads0) << 8) | ((opr0) << 10) | ((op1) << 16) | ((pads1) << 24) | ((opr1) << 26))
#define LUT0(op0, pads0, opr0, op1, pads1, opr1) ((INSTR(op1, pads1, opr1) << 16) | INSTR(op0, pads0, opr0))

#define LUT_INDEX_READ  0
#define LUT_INDEX_WRITE 4
#define LUT_INDEX_WREN  8

int transfer_blocking(FlexSPI_Type *fspi, flexspi_transfer_t *xfer)
{
    int result = 0;

    uint32_t configValue = 0;

    /* Clear sequence pointer before sending data to external devices. */
    fspi->FLSHCR2[xfer->port] |= (1 << 31);

    /* Clear former pending status before start this transfer. */
    clear_flags(fspi);

    /* Configure fspi address. */
    fspi->IPCR0 = xfer->deviceAddress;

    /* Reset fifos. */
    fspi->IPTXFCR |= 1; // Flush TX FIFO
    fspi->IPRXFCR |= 1; // Flush RX FIFO

    /* Configure data size. */
    if ((xfer->cmdType == kFLEXSPI_Read) || (xfer->cmdType == kFLEXSPI_Write) || (xfer->cmdType == kFLEXSPI_Config))
    {
        slogt("Data size: %d", xfer->dataSize);
        configValue = xfer->dataSize;
    }

    /* Configure sequence ID. */
    configValue |= (xfer->seqIndex << 16) | ((xfer->SeqNumber - 1U) << 24);
    fspi->IPCR1 = configValue;

    /* Start Transfer. */
    fspi->IPCMD |= 1;

    if ((xfer->cmdType == kFLEXSPI_Write) || (xfer->cmdType == kFLEXSPI_Config))
    {
        // slogt("Writing %d bytes...", xfer->dataSize);
        result = write_blocking(fspi, (uint8_t *)xfer->data, xfer->dataSize);
        // slogt("Write completed.");
    }
    else if (xfer->cmdType == kFLEXSPI_Read)
    {
        slogt("Reading %d bytes...", xfer->dataSize);
        result = read_blocking(fspi, (uint8_t *)xfer->data, xfer->dataSize);
        slogt("Read completed.");
    }
    else
    {
        /* Empty else. */
        assert(false);
    }

    /* Wait until the IP command execution finishes */
    // LOG_REGISTER(&fspi->INTR, FLEXSPI_BASE + offsetof(FlexSPI_Type, INTR));
    // LOG_REGISTER(&fspi->STS0, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS0));
    // LOG_REGISTER(&fspi->STS1, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS1));
    // LOG_REGISTER(&fspi->STS2, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS2));
    slogt("Waiting for command completion...");
    while (0UL == (fspi->INTR & (1 << 0)))
    {
    }

    /* Unless there is an error status already set, capture the latest one */
    if (result == 0)
    {
        fspi->INTR |= (1 << 6); // Clear IPTXFEMPTY flag
    }

    return result;
}

void QSPI_Init()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = segfault_sigaction;
    sa.sa_flags     = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    qspi_ctx.fd        = -1;
    qspi_ctx.page_size = sysconf(_SC_PAGE_SIZE);
    qspi_ctx.init_done = 0;

    assert(qspi_ctx.page_size > 0);
    assert(qspi_ctx.page_size % 4096 == 0);
    // open /dev/mem to access physical memory

    slogt("opening /dev/mem...");
    qspi_ctx.fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (qspi_ctx.fd < 0)
    {
        slogt("Failed to open /dev/mem: %s", strerror(errno));
        return;
    }

    slogt("/dev/mem opened successfully");
    // Request memory mappings for FlexSPI
    if (request_flexspi_memory(&qspi_ctx) != 0)
    {
        slogt("Failed to request FlexSPI memory");
        close(qspi_ctx.fd);
        qspi_ctx.fd = -1;
        return;
    }


    slogt("IOMUX initialization...");
    iomux_init(&qspi_ctx);
    slogt("IOMUX initialized.");

    qspi_ctx.flexspi->MCR0 |= FSPI_MCR0_MDIS; // Disable FlexSPI
    clock_init(&qspi_ctx, clk_mux, pre_div, post_div);

    qspi_ctx.flexspi->MCR0 |= (1 << 1);            // Disable FlexSPI
    qspi_ctx.flexspi->INTEN = (1 << 6) | (1 << 0); // Enable IPTX FIFO empty interrupt
    // qspi_ctx.flexspi->MCR0 |= FSPI_MCR0_SWRST;     // Software reset
    // setup_lut(qspi_ctx.flexspi);
    qspi_ctx.flexspi->MCR0 &= ~FSPI_MCR0_MDIS; // Enable FlexSPI
    qspi_ctx.init_done = 1;
}

void QSPI_SetupLut(uint32_t *lut, size_t len)
{
    FlexSPI_Type *fspi = qspi_ctx.flexspi;
    assert(fspi != NULL);
    assert(lut != NULL);
    assert(len <= lengthof(fspi->LUT) * sizeof(uint32_t));

    slogi("Setting up LUT...");

    unlock_lut(fspi);
    memcpy(&fspi->LUT[0], lut, len);
    lock_lut(fspi);

    slogi("LUT setup completed.");
}

int QSPI_IsInitialized()
{
    return qspi_ctx.init_done;
}

int QSPI_Busy(void)
{
    slogi("QSPI_Busy check");
    if (!qspi_ctx.init_done)
    {
        slogf("QSPI is not initialized");
        return 1;
    }

    LOG_REGISTER(&qspi_ctx.flexspi->INTR, FLEXSPI_BASE + offsetof(FlexSPI_Type, INTR));
    LOG_REGISTER(&qspi_ctx.flexspi->STS0, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS0));
    LOG_REGISTER(&qspi_ctx.flexspi->STS1, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS1));
    LOG_REGISTER(&qspi_ctx.flexspi->STS2, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS2));
    uint32_t intr = qspi_ctx.flexspi->INTR;

    if (intr & (1 << 3))
    {
        slogf("QSPI error: IP RX FIFO underflow");
        return -1;
    }

    if (intr & 0x1)
    {
        // Command done
        clear_flags(qspi_ctx.flexspi);
        return 0;
    }

    return 1;
}
void QSPI_DeInit()
{
    if (!qspi_ctx.init_done)
    {
        slogi("QSPI is not initialized, nothing to deinitialize");
        return;
    }

    // Unmap FlexSPI registers
    munmap(qspi_ctx.map_iomux, PAGE_SIZE_64K);
    munmap(qspi_ctx.map_fspi, sizeof(FlexSPI_Type));
    munmap(qspi_ctx.map_ccm, PAGE_SIZE_64K);
    munmap(qspi_ctx.map_rdc, PAGE_SIZE_64K);
    close(qspi_ctx.fd);
    qspi_ctx.fd        = -1;
    qspi_ctx.init_done = 0;
    slogt("QSPI deinitialized successfully");
}

int QSPI_Write(uint32_t addr, uint8_t lut_index, uint8_t *buffer, size_t size)
{
    // assert(buffer && size > 0);

    slogi("QSPI_Write: addr=0x%08X, size=%zu", addr, size);

    flexspi_transfer_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.deviceAddress = addr;
    xfer.port          = kFlexSPI_PortA1;
    xfer.cmdType       = kFLEXSPI_Write;
    xfer.seqIndex      = lut_index;
    xfer.SeqNumber     = 1;
    xfer.data          = (uint32_t *)buffer;
    xfer.dataSize      = size;

    int ret = transfer_blocking(qspi_ctx.flexspi, &xfer);

    LOG_REGISTER(&qspi_ctx.flexspi->STS0, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS0));
    LOG_REGISTER(&qspi_ctx.flexspi->STS1, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS1));
    LOG_REGISTER(&qspi_ctx.flexspi->INTR, FLEXSPI_BASE + offsetof(FlexSPI_Type, INTR));

    return ret;
}

int QSPI_Read(uint32_t addr, uint8_t *buffer, size_t size)
{
    assert(buffer && size > 0);

    slogi("QSPI_Read: addr=0x%08X, size=%zu", addr, size);

    flexspi_transfer_t xfer;
    xfer.deviceAddress = addr;
    xfer.port          = kFlexSPI_PortA1;
    xfer.cmdType       = kFLEXSPI_Read;
    xfer.seqIndex      = LUT_INDEX_READ;
    xfer.SeqNumber     = 1;
    xfer.data          = (uint32_t *)buffer;
    xfer.dataSize      = size;

    int ret = transfer_blocking(qspi_ctx.flexspi, &xfer);

    LOG_REGISTER(&qspi_ctx.flexspi->STS0, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS0));
    LOG_REGISTER(&qspi_ctx.flexspi->STS1, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS1));
    LOG_REGISTER(&qspi_ctx.flexspi->INTR, FLEXSPI_BASE + offsetof(FlexSPI_Type, INTR));

    return ret;
}

int QSPI_ReadSample(uint32_t addr, void *sample, size_t size)
{
    assert(sample && size > 0);

    slogi("QSPI_ReadSample: addr=0x%08X, size=%zu", addr, size);

    flexspi_transfer_t xfer;
    xfer.deviceAddress = addr;
    xfer.port          = kFlexSPI_PortA1;
    xfer.cmdType       = kFLEXSPI_Read;
    xfer.seqIndex      = FPGA_LUT_IDX_RD_SAMPLE;
    xfer.SeqNumber     = 1;
    xfer.data          = (uint32_t *)sample;
    xfer.dataSize      = size;

    int ret = transfer_blocking(qspi_ctx.flexspi, &xfer);

    LOG_REGISTER(&qspi_ctx.flexspi->STS0, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS0));
    LOG_REGISTER(&qspi_ctx.flexspi->STS1, FLEXSPI_BASE + offsetof(FlexSPI_Type, STS1));
    LOG_REGISTER(&qspi_ctx.flexspi->INTR, FLEXSPI_BASE + offsetof(FlexSPI_Type, INTR));

    return ret;
}
