#include "qspi.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "internals.h"

#include "slog.h"

// clang-format off
#define CCM_BASE                0x30380000  // Base address for Clock Control Module
#define CCM_CCGR_BASE           0x30384000  // Base address for Clock Gating Registers
#define CCM_CCGR_OFFSET         0x4000  // Base address for Clock Gating Registers
// 8000 - 3038_0000 3038_AB80
#define CCM_TARGET_ROOT_BASE    0x30380000  // Base address for Target Root Registers
#define CCM_LUT_BASE            0x30BB0000  // Base address for LUT Key Register

#define CCM_CCGR47_OFFSET           0x02F0  // Offset for Clock Gating Register 47
#define CCM_TARGET_ROOT87_OFFSET    0xAB80  // Offset for Target Root Register

#define CCM_CCGR47_A53_VALUE    0x03 // Value to enable A53 clock in CCGR47

#define CCM_TARGET_ROOT87_ENABLE (1 << 28)
#define CCM_TARGET_ROOT87_CLOCK_SOURCE (1 << 24)


// Interrupt Enable Register Bit Masks
#define LUT_LOCK        (0x01)
#define LUT_UNLOCK      (0x02)

#define LUT_KEY_VALUE   (0x5AF05AF0) // Key value to unlock the LUT

#define LUT_INTEN_OFFSET         (0x10) // Offset for Interrupt Enable Register
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


#define LUT_MR0_OFFSET             (0x00) // Memory Register 0
#define LUT_KEY18_OFFSET            0x18    // Offset for LUT Key Register
#define LUT_CR_OFFSET               0x1C    // Offset for LUT Key Register
#define LUT_INTR_OFFSET             (0x14) // Interrupt Enable Register
#define LUT_IPCR0_OFFSET            (0xA0)
#define LUT_IPCR1_OFFSET            (0xA4)
#define LUT_IPTXFCR_OFFSET          (0x08) // IP TX FIFO Control Register
#define LUT_IPCMD_OFFSET            (0xB0) // IP TX FIFO Control Register
#define LUT_IP_TX_FIFO_CR_OFFSET    (0xBC) // TX IP FIFO Control Register
#define LUT_IP_TX_FIFO_DR_OFFSET    (0x180) // TX IP FIFO Register Data Register
#define LUT_IP_RX_FIFO_CR_OFFSET    (0xB8) // RX IP FIFO Control Register
#define LUT_IP_RX_FIFO_DR_OFFSET    (0x100) // RX IP FIFO Register Data Register

// clang-format on
typedef struct QSPI_Context
{
    int fd; // File descriptor for /dev/mem
    int init_done;
    long int page_size; // Page size for memory mapping
    // Initialization done flag
    uint32_t *QSPI_CCGR_BASE;   // Base address for Clock Gating Registers
    uint32_t *QSPI_TARGET_BASE; // Base address for Target Root Registers
    uint32_t *QSPI_LUT_BASE;    // LUT Key Register
} QSPI_Context;

typedef struct QSPI_Registers
{
    int init_done;                // Initialization done flag
    uint32_t *QSPI_CCGR47;        // Clock Gating Register
    uint32_t *QSPI_TARGET_ROOT87; // Target Root Register
    uint32_t *QSPI_INTEN;         // Interrupt Enable Register
    uint32_t *QSPI_INTR;          // Interrupt Register
    uint32_t *QSPI_MR0;           // Memory Register 0
    uint32_t *QSPI_LUTKEY;        // LUT Key Register
    uint32_t *QSPI_LUTCR;         // LUT Control Register
    uint32_t *QSPI_TX_IP_FIFO_CR; // TX IP FIFO Control Register
    uint32_t *QSPI_TX_IP_FIFO_DR; // TX IP FIFO Register Data Register
    uint32_t *QSPI_RX_IP_FIFO_CR; // RX IP FIFO Control Register
    uint32_t *QSPI_RX_IP_FIFO_DR; // RX IP FIFO Register Data Register
    uint32_t *QSPI_IPCR0;         // IP Command Register 0
    uint32_t *QSPI_IPCR1;         // IP Command Register 1
    uint32_t *QSPI_IPCMD;         // IP Command Register
    uint32_t *QSPI_IPTXFCR;       // IP TX FIFO Control Register
} QSPI_Registers;

STATIC QSPI_Context qspi_ctx = {};
STATIC QSPI_Registers qspi_regs = {};

STATIC int require_memory(QSPI_Context *ctx)
{
    int fd = ctx->fd;
    ctx->page_size = sysconf(_SC_PAGESIZE);
    ctx->QSPI_CCGR_BASE = (uint32_t *)mmap((void *)CCM_CCGR_BASE, ctx->page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // ctx->QSPI_CCGR_BASE = (uint32_t *)mmap((void *)CCM_BASE, ctx->page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
    // 0);
    if (ctx->QSPI_CCGR_BASE == MAP_FAILED)
    {
        slogt("Failed to map QSPI CCGR base: %s", strerror(errno));
        return -1;
    }

    ctx->QSPI_TARGET_BASE = (uint32_t *)mmap((void *)(CCM_TARGET_ROOT_BASE + CCM_CCGR47_OFFSET), ctx->page_size,
                                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ctx->QSPI_TARGET_BASE == MAP_FAILED)
    {
        slogt("Failed to map QSPI Target Root base: %s", strerror(errno));
        // Clean up previously mapped memory
        munmap(ctx->QSPI_CCGR_BASE, ctx->page_size);
        return -1;
    }

    ctx->QSPI_LUT_BASE =
        (uint32_t *)mmap((void *)CCM_LUT_BASE, ctx->page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ctx->QSPI_LUT_BASE == MAP_FAILED)
    {
        slogt("Failed to map QSPI LUT base: %s", strerror(errno));
        // Clean up previously mapped memory
        munmap(ctx->QSPI_CCGR_BASE, ctx->page_size);
        munmap(ctx->QSPI_TARGET_BASE, ctx->page_size);
        return -1;
    }
    ctx->init_done = 1;
    return 0;
}
void QSPI_Init()
{
    // Clean up previous state
    memset(&qspi_ctx, 0, sizeof(QSPI_Context));
    memset(&qspi_regs, 0, sizeof(QSPI_Registers));

    // open /dev/mem to access physical memory
    qspi_ctx.fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (qspi_ctx.fd < 0)
    {
        slogt("Failed to open /dev/mem: %s", strerror(errno));
        return;
    }

    // Map the required memory regions
    if (require_memory(&qspi_ctx) < 0)
    {
        slogt("Failed to map required memory regions for QSPI");
        close(qspi_ctx.fd);
        return;
    }

    // Check if initialization is already done
    assert(qspi_ctx.init_done == 1 && "QSPI context initialization failed");
    assert(qspi_ctx.QSPI_CCGR_BASE == (uint32_t *)CCM_CCGR_BASE && "QSPI CCGR base mapping failed");
    assert(qspi_ctx.QSPI_TARGET_BASE == (uint32_t *)(CCM_TARGET_ROOT_BASE + CCM_TARGET_ROOT87_OFFSET) &&
           "QSPI Target Root base mapping failed");
    assert(qspi_ctx.QSPI_LUT_BASE == (uint32_t *)CCM_LUT_BASE && "QSPI LUT base mapping failed");

    qspi_ctx.QSPI_CCGR_BASE[CCM_CCGR_OFFSET + CCM_CCGR47_OFFSET] = CCM_CCGR47_A53_VALUE;
    slogt("QSPI clock enabled: %08x:%08x", &qspi_ctx.QSPI_CCGR_BASE[CCM_CCGR_OFFSET + CCM_CCGR47_OFFSET],
          qspi_ctx.QSPI_CCGR_BASE[CCM_CCGR_OFFSET + CCM_CCGR47_OFFSET]);
    //
    /// Step 2: Set the clock root register CCM_TARGET_ROOT87 with address 0x3038AB80 to determine the flexspi clock
    /// frequency (Page 459). The CCM_TARGET_ROOT87[MUX] determines the clock source (Page 239). Please note that bit 28
    /// must be set to 1.
    qspi_ctx.QSPI_CCGR_BASE[CCM_CCGR_OFFSET + CCM_TARGET_ROOT87_OFFSET] =
        CCM_TARGET_ROOT87_ENABLE | CCM_TARGET_ROOT87_CLOCK_SOURCE;
    // *qspi_ctx.QSPI_TARGET_BASE = CCM_TARGET_ROOT87_ENABLE | CCM_TARGET_ROOT87_CLOCK_SOURCE;
    slogt("QSPI Target Root Register set: %08x:%08x", qspi_ctx.QSPI_TARGET_BASE, *qspi_ctx.QSPI_TARGET_BASE);

    // /// Step 3: Set the associated bits in interrupt enable register with address 0x30BB0010 (Page 2458).
    // // Set Interupt Enable (RX, TX , empty, AHB, IP, CMD, DONE)
    // // clang-format off
    // qspi_regs.QSPI_INTEN[0] =    ITEN_SEQTIMEOUTN |
    //                             ITEN_AHBBUSTIMEOUTEN |
    //                             ITEN_SCKSTOPBYWREN |
    //                             ITEN_SCKSTOPBYRDEN |
    //                             ITEN_DATALEARNFAILEN |
    //                             ITEN_IPTXWEEN |
    //                             ITEN_IPRXWAEN |
    //                             ITEN_AHBCMDERRREN |
    //                             ITEN_IPCMDERRREN |
    //                             ITEN_AHBCMDGEEN |
    //                             ITEN_IPCMDGEEN |
    //                             ITEN_IPCMDDONEEN;
    qspi_ctx.QSPI_LUT_BASE[LUT_INTEN_OFFSET] = ITEN_SEQTIMEOUTN | ITEN_AHBBUSTIMEOUTEN | ITEN_SCKSTOPBYWREN |
                                               ITEN_SCKSTOPBYRDEN | ITEN_DATALEARNFAILEN | ITEN_IPTXWEEN |
                                               ITEN_IPRXWAEN | ITEN_AHBCMDERRREN | ITEN_IPCMDERRREN | ITEN_AHBCMDGEEN |
                                               ITEN_IPCMDGEEN | ITEN_IPCMDDONEEN;
    // // clang-format on
    slogt("QSPI Interrupt Enable Register set: %08x:%08x", &qspi_ctx.QSPI_LUT_BASE[LUT_INTEN_OFFSET],
          qspi_ctx.QSPI_LUT_BASE[LUT_INTEN_OFFSET]);
    //
    // /// Step 4: Enable the flexspi by setting MCR0[MDIS] to 0 (Page 2450).
    // // Enable the flexspi by setting MCR0[MDIS] to 0 (Page 2450).
    qspi_ctx.QSPI_LUT_BASE[LUT_MR0_OFFSET] &= ~(1 << 1); // Clear the MDIS bit to enable the module
    slogt("QSPI Memory Register 0 set: %08x:%08x", &qspi_ctx.QSPI_LUT_BASE[LUT_MR0_OFFSET],
          qspi_ctx.QSPI_LUT_BASE[LUT_MR0_OFFSET]);
    //
    // /// Step 5: Unlock Look Up Table (LUT). LUTKEY (0x30BB0018) and LUTCR (0x30BB001C).
    // qspi_regs.QSPI_LUTKEY[0] = LUT_KEY_VALUE; // Write the key value to unlock
    // qspi_regs.QSPI_LUTCR[0] = LUT_UNLOCK;     // Set the LUTCR to unlock
    // slogt("QSPI LUT Lock state: %08x:%08x", (unsigned int)qspi_regs.QSPI_LUTCR, qspi_regs.QSPI_LUTCR[0]);
    qspi_ctx.QSPI_LUT_BASE[LUT_KEY18_OFFSET] = LUT_KEY_VALUE; // Write the key value to unlock
    qspi_ctx.QSPI_LUT_BASE[LUT_CR_OFFSET] = LUT_UNLOCK;       // Set the LUTCR to unlock
    slogt("QSPI LUT Lock state: %08x:%08x", &qspi_ctx.QSPI_LUT_BASE[LUT_CR_OFFSET],
          qspi_ctx.QSPI_LUT_BASE[LUT_CR_OFFSET]);

    slogi("QSPI initialization completed successfully");
}

int QSPI_IsInitialized()
{
    return qspi_ctx.init_done;
}

void QSPI_DeInit()
{
    if (qspi_ctx.init_done)
    {
        // Clean up memory mappings
        if (qspi_ctx.QSPI_CCGR_BASE)
            munmap(qspi_ctx.QSPI_CCGR_BASE, qspi_ctx.page_size);
        if (qspi_ctx.QSPI_TARGET_BASE)
            munmap(qspi_ctx.QSPI_TARGET_BASE, qspi_ctx.page_size);
        if (qspi_ctx.QSPI_LUT_BASE)
            munmap(qspi_ctx.QSPI_LUT_BASE, qspi_ctx.page_size);

        qspi_ctx.init_done = 0;
        slogt("QSPI resources cleaned up");
    }
}

int QSPI_Transmit(const uint8_t *data, size_t length)
{
    if (!QSPI_IsInitialized())
    {
        slogf("QSPI is not initialized");
        return -1;
    }

    if (data == NULL || length == 0)
    {
        slogf("Invalid data or length for QSPI transmission");
        return -1;
    }

    slogt("Transmitting %zu bytes over QSPI", length);

    return 0; // Return 0 on success
}
