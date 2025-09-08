#include "qspi.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "flexspi.h"
#include "internals.h"

#include "slog.h"

typedef struct QSPI_Context
{
    int           fd;
    int           init_done;
    FlexSPI_Type *flexspi;

} QSPI_Context;

static const uint32_t pre_div  = 1; // Pre-divider value (1-8)
static const uint32_t post_div = 5; // Post-divider value (1-64
static const uint32_t clk_mux  = 1; // Clock mux value (see _ccm_rootmux_xxx enumeration)

static QSPI_Context qspi_ctx;

static int request_flexspi_memory(QSPI_Context *ctx)
{
    ctx->page_size = sysconf(_SC_PAGE_SIZE);
    if (ctx->page_size < 0)
    {
        slogf("Failed to get system page size: %s", strerror(errno));
        return -1;
    }

    // Map the FlexSPI control registers
    ctx->QSPI_TARGET_BASE = mmap(NULL, ctx->page_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, FLEXSPI_BASE);
    if (ctx->QSPI_TARGET_BASE == MAP_FAILED)
    {
        slogf("Failed to mmap FlexSPI base: %s", strerror(errno));
        return -1;
    }
    ctx->flexspi = (FlexSPI_Type *)ctx->QSPI_TARGET_BASE;

    return 0;
}

static inline void unlock_lut(FlexSPI_Type *fspi)
{
    // Unlock LUT for update
    fspi->LUTKEY = FPSI_LUT_KEY_VALUE;
    fspi->LUTCR  = FPSI_LUT_UNLOCK;
}
//
static inline void lock_lut(FlexSPI_Type *fspi)
{
    // Lock LUT after update
    fspi->LUTKEY = FPSI_LUT_KEY_VALUE;
    fspi->LUTCR  = FPSI_LUT_LOCK;
}

static inline void clear_ip_done(FlexSPI_Type *fspi)
{
    fspi->INTR = 0x1; // Clear IP command done interrupt
}

static inline void copy_buffer_to_flexspi(FlexSPI_Type *fspi, const uint8_t *buffer, size_t size){
    memcpy((void*)&fspi->TFDR, buffer, size);
}
static inline void exec_command(FlexSPI_Type *fspi, int seqid, int seqnum, int datasize, uint32_t addr)
{
    fspi->IPCR0 = addr;
    fspi->IPCR1 = (seqid << 16) | (seqnum << 24) | datasize;
    fspi->IPCMD = 1;
    // wait_ip_done(regs);
}
static void update_lut(QSPI_Registers *regs, uint32_t index, const uint32_t *cmd, uint32_t count)
{
    int       i;
    uint32_t *lutBase;
    assert(index < FSPI_LUT_NUM && "LUT index out of bounds");

    /* Unlock LUT for update. */
    unlock_lut(regs);

    lutBase = (uint32_t *)&regs->QSPI_LUT[index];
    for (i = 0; i < count; i++)
    {
        *lutBase++ = *cmd++;
    }

    /* Lock LUT. */
    lock_lut(regs);
}

static void qspi_clock_init(QSPI_Context *ctx, uint32_t mux, uint32_t pre, uint32_t post)
{
    volatile uint32_t *ccm_base = mmap(NULL, (64 * 1024), PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, CCM_BASE);
    if (ccm_base == MAP_FAILED)
    {
        slogt("Failed to mmap CCM base: %s", strerror(errno));
        return;
    }
    // Enable QSPI clock

    /* Domain clocks needed all the time */
    *(ccm_base + CCM_CCGR47 / 4) = CCM_CCGR47;
    // Selection of root clock sources and set pre/post divider
    *(ccm_base + QSPI_CLK_ROOT / 4) = CLK_ROOT_EN | MUX_CLK_ROOT_SELECT(mux) | PRE_PODF(pre - 1) | POST_PODF(post - 1);
    munmap((void *)ccm_base, (64 * 1024));
}

static qspi_setup_lut(FlexSPI_Type *fspi)
{
    unlock_lut(fspi);

    fspi->LUT[0] = FlexSPI_LUT_SEQ(CMD_SDR, kFlexSPI_4PAD, 0x06, STOP, kFlexSPI_4PAD, 0);
}

void QSPI_Init()
{
    // Clean up previous state
    memset(&qspi_ctx, 0, sizeof(QSPI_Context));

    // open /dev/mem to access physical memory
    qspi_ctx.fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (qspi_ctx.fd < 0)
    {
        slogt("Failed to open /dev/mem: %s", strerror(errno));
        return;
    }

    // Request memory mappings for FlexSPI
    if (request_flexspi_memory(&qspi_ctx) != 0)
    {
        slogt("Failed to request FlexSPI memory");
        close(qspi_ctx.fd);
        qspi_ctx.fd = -1;
        return;
    }

    // Initialize QSPI clock
    qspi_clock_init(&qspi_ctx, clk_mux, pre_div, post_div);
    qspi_ctx.init_done = 1;
    slogt("QSPI initialized successfully");
}

int QSPI_IsInitialized()
{
    return qspi_ctx.init_done;
}

int QSPI_Busy(void)
{
    return !(qspi_ctx.flexspi->INTR & 0x1);
}
void QSPI_DeInit()
{
    if (!qspi_ctx.init_done)
    {
        slogi("QSPI is not initialized, nothing to deinitialize");
        return;
    }

    // Unmap FlexSPI registers
    munmap(qspi_ctx.flexspi, sizeof(FlexSPI_Type));
    close(qspi_ctx.fd);
    qspi_ctx.fd        = -1;
    qspi_ctx.init_done = 0;
    slogt("QSPI deinitialized successfully");
}

void QSPI_Write(uint32_t addr, uint8_t *buffer, size_t size)
{
    uint32_t status;

    if (!QSPI_IsInitialized())
    {
        slogf("qspi is not initialized");
        return;
    }
    if (size > 256)
    {
        slogf("qspi write size too large");
        return;
    }
    
    copy_buffer_to_flexspi(qspi_ctx.flexspi, buffer, size);
    exec_command(qspi_ctx.flexspi, CMD_WRITE, 1, size)
}
