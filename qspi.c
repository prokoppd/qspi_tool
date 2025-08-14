#include "qspi.h"

#include <stdint.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "slog.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

#define REG_SIZE 0x1000 // Size of the region to map (page aligned)

// clang-format off
#define CCM_CCGR_BASE           0x30384000  // Base address for Clock Gating Registers
#define CCM_TARGET_ROOT_BASE    0x30388000  // Base address for Target Root Registers
#define CCM_LUT_BASE            0x30BB0000  // Base address for LUT Key Register

#define CCM_CCGR47_OFFSET           0x02F0  // Offset for Clock Gating Register 47
#define CCM_TARGET_ROOT87_OFFSET    0xAB80  // Offset for Target Root Register
#define LUT_KEY18_OFFSET            0x18    // Offset for LUT Key Register
#define LUT_CR_OFFSET               0x1C    // Offset for LUT Key Register

#define CCM_CCGR47_A53_VALUE    0x03 // Value to enable A53 clock in CCGR47

#define CCM_TARGET_ROOT87_ENABLE (1 << 28)
#define CCM_TARGET_ROOT87_CLOCK_SOURCE (1 << 24)


// Interrupt Enable Register Bit Masks
#define LUT_ITEN_OFFSET         (0x10) // Offset for Interrupt Enable Register
#define ITEN_SEQTIMEOUTN        (1U << 11)  // Sequence execution timeout interrupt enable
#define ITEN_AHBBUSTIMEOUTEN    (1U << 10) // AHB Bus timeout interrupt
#define ITEN_SCKSTOPBYWREN      (1U << 9)  // SCLK stopped during command sequence because Async TX FIFO empty interrupt enable
#define ITEN_SCKSTOPBYRDEN      (1U << 8)  // SCLK stopped during command sequence because Async RX FIFO full interrupt enable
#define ITEN_DATALEARNFAILEN    (1U << 7)  // Data Learning failed interrupt enable
#define ITEN_IPTXWEEN           (1U << 6) // IP TX FIFO WaterMark empty interrupt enable
#define ITEN_IPRXWAEN           (1U << 5) // IP RX FIFO WaterMark available interrupt enable
#define ITEN_AHBCMDERRREN       (1U << 4) // AHB triggered Command Sequences Error Detected interrupt enable
#define ITEN_IPCMDERRREN        (1U << 3) // IP triggered Command Sequences Error Detected interrupt enable
#define ITEN_AHBCMDGEEN         (1U << 2) // AHB triggered Command Sequences Grant Timeout interrupt enable
#define ITEN_IPCMDGEEN          (1U << 1) // IP triggered Command Sequences Grant Timeout interrupt enable
#define ITEN_IPCMDDONEEN        (1U << 0) // IP triggered Command Sequences Execution finished interrupt enable

#define LUT_LOCK        (0x01)
#define LUT_UNLOCK      (0x02)
#define LUT_KEY_VALUE   (0x5AF05AF0) // Key value to unlock the LUT

// clang-format on
typedef struct QSPI_Context
{
    int init_done;          // Initialization done flag
    void *QSPI_CCGR_BASE;   // Base address for Clock Gating Registers
    void *QSPI_TARGET_BASE; // Base address for Target Root Registers
    void *QSPI_LUT_BASE;    // LUT Key Register
} QSPI_Context;

typedef struct QSPI_Registers
{
    int init_done;                // Initialization done flag
    uint32_t *QSPI_CCGR47;        // Clock Gating Register
    uint32_t *QSPI_TARGET_ROOT87; // Target Root Register
    uint32_t *QSPI_ITEN;          // Interrupt Enable Register
    uint32_t *QSPI_MR0;           // Memory Register 0 (not used in this example)
    uint32_t *QSPI_LUTKEY;        // LUT Key Register
    uint32_t *QSPI_LUTCR;         // LUT Control Register
} QSPI_Registers;

static QSPI_Context qspi_ctx = {};
static QSPI_Registers qspi_regs = {};

static int require_memory(QSPI_Context *ctx, int fd)
{
    ctx->QSPI_CCGR_BASE = mmap(NULL, REG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CCM_CCGR_BASE);
    if (ctx->QSPI_CCGR_BASE == MAP_FAILED)
    {
        sloge("Failed to map QSPI CCGR base: %s", strerror(errno));
        return -1;
    }

    ctx->QSPI_TARGET_BASE = mmap(NULL, REG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CCM_TARGET_ROOT_BASE);
    if (ctx->QSPI_TARGET_BASE == MAP_FAILED)
    {
        sloge("Failed to map QSPI Target Root base: %s", strerror(errno));
        // Clean up previously mapped memory
        munmap(ctx->QSPI_CCGR_BASE, REG_SIZE);
        return -1;
    }

    ctx->QSPI_LUT_BASE = mmap(NULL, REG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CCM_LUT_BASE);
    if (ctx->QSPI_LUT_BASE == MAP_FAILED)
    {
        sloge("Failed to map QSPI LUT base: %s", strerror(errno));
        // Clean up previously mapped memory
        munmap(ctx->QSPI_CCGR_BASE, REG_SIZE);
        munmap(ctx->QSPI_TARGET_BASE, REG_SIZE);
        return -1;
    }
    ctx->init_done = 1;
    return 0;
}

static void map_registers(QSPI_Registers *regs, QSPI_Context *ctx)
{
    regs->QSPI_CCGR47 = (uint32_t *)(ctx->QSPI_CCGR_BASE + CCM_CCGR47_OFFSET);
    regs->QSPI_TARGET_ROOT87 = (uint32_t *)(ctx->QSPI_TARGET_BASE + CCM_TARGET_ROOT87_OFFSET);
    regs->QSPI_LUTKEY = (uint32_t *)(ctx->QSPI_LUT_BASE + LUT_KEY18_OFFSET);
    regs->QSPI_LUTCR = (uint32_t *)(ctx->QSPI_LUT_BASE + LUT_CR_OFFSET);
    regs->QSPI_ITEN = (uint32_t *)(ctx->QSPI_LUT_BASE + LUT_ITEN_OFFSET);
    regs->QSPI_MR0 = (uint32_t *)(ctx->QSPI_LUT_BASE + 0x00); // Memory Register 0

    regs->init_done = 1;
}

void QSPI_Init()
{
    // Clean up previous state
    memset(&qspi_ctx, 0, sizeof(QSPI_Context));
    memset(&qspi_regs, 0, sizeof(QSPI_Registers));

    // open /dev/mem to access physical memory
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        slogf("Failed to open /dev/mem: %s", strerror(errno));
        return;
    }

    // Map the required memory regions
    if (require_memory(&qspi_ctx, fd) < 0)
    {
        slogf("Failed to map required memory regions for QSPI");
        close(fd);
        return;
    }

    // Check if initialization is already done
    assert(qspi_ctx.init_done == 1 && "QSPI context initialization failed");
    assert(qspi_ctx.QSPI_CCGR_BASE != NULL && "QSPI CCGR base mapping failed");
    assert(qspi_ctx.QSPI_TARGET_BASE != NULL && "QSPI Target Root base mapping failed");
    assert(qspi_ctx.QSPI_LUT_BASE != NULL && "QSPI LUT base mapping failed");

    // Map the registers
    map_registers(&qspi_regs, &qspi_ctx);

    // Check if initialization is already done
    assert(qspi_regs.init_done == 1 && "QSPI registers mapping failed");
    assert(qspi_regs.QSPI_CCGR47 != NULL && "QSPI CCGR47 register mapping failed");
    assert(qspi_regs.QSPI_TARGET_ROOT87 != NULL && "QSPI Target Root Register mapping failed");
    assert(qspi_regs.QSPI_LUTKEY != NULL && "QSPI LUT Key Register mapping failed");
    assert(qspi_regs.QSPI_LUTCR != NULL && "QSPI LUT Control Register mapping failed");

    // Enable QSPI clock
    qspi_regs.QSPI_CCGR47[0] = CCM_CCGR47_A53_VALUE;
    slogt("QSPI clock enabled: %08x:%08x", (unsigned int)qspi_regs.QSPI_CCGR47, qspi_regs.QSPI_CCGR47[0]);

    // Set QSPI Speed
    qspi_regs.QSPI_TARGET_ROOT87[0] = CCM_TARGET_ROOT87_ENABLE | CCM_TARGET_ROOT87_CLOCK_SOURCE;
    slogt("QSPI Target Root Register set: %08x:%08x", (unsigned int)qspi_regs.QSPI_TARGET_ROOT87,
          qspi_regs.QSPI_TARGET_ROOT87[0]);

    // Set Interupt Enable (RX, TX , empty, AHB, IP, CMD, DONE)
    // clang-format off
    qspi_regs.QSPI_ITEN[0] =    ITEN_SEQTIMEOUTN | 
                                ITEN_AHBBUSTIMEOUTEN | 
                                ITEN_SCKSTOPBYWREN | 
                                ITEN_SCKSTOPBYRDEN | 
                                ITEN_DATALEARNFAILEN | 
                                ITEN_IPTXWEEN | 
                                ITEN_IPRXWAEN | 
                                ITEN_AHBCMDERRREN | 
                                ITEN_IPCMDERRREN | 
                                ITEN_AHBCMDGEEN | 
                                ITEN_IPCMDGEEN | 
                                ITEN_IPCMDDONEEN;
    // clang-format on
    slogt("QSPI Interrupt Enable Register set: %08x:%08x", (unsigned int)qspi_regs.QSPI_ITEN, qspi_regs.QSPI_ITEN[0]);

    // Enable the flexspi by setting MCR0[MDIS] to 0 (Page 2450).
    qspi_regs.QSPI_MR0[0] &= ~(1 << 1); // Clear the MDIS bit to enable the module
    slogt("QSPI Memory Register 0 set: %08x:%08x", (unsigned int)qspi_regs.QSPI_MR0, qspi_regs.QSPI_MR0[0]);

    // Unlock Look Up Table (LUT). LUTKEY (0x30BB0018) and LUTCR (0x30BB001C).
    qspi_regs.QSPI_LUTKEY[0] = LUT_KEY_VALUE; // Write the key value to unlock
    qspi_regs.QSPI_LUTCR[0] = LUT_UNLOCK;     // Set the LUTCR to unlock
    slogt("QSPI LUT Lock state: %08x:%08x", (unsigned int)qspi_regs.QSPI_LUTCR, qspi_regs.QSPI_LUTCR[0]);
}
