#include "qspi.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "internals.h"
#include "flexspi.h"


#include "slog.h"

// clang-format off
#define CCM_CCGR_BASE           0x30384000  // Base address for Clock Gating Registers
#define CCM_CCGR_OFFSET         0x4000  // Base address for Clock Gating Registers
// 8000 - 3038_0000 3038_AB80
// #define CCM_TARGET_ROOT_BASE    0x30380000  // Base address for Target Root Registers
#define CCM_TARGET_ROOT_BASE    0x3038A000  // Base address for Target Root Registers
#define CCM_LUT_BASE            0x30BB0000  // Base address for LUT Key Register

#define CCM_CCGR47_OFFSET           0x02F0  // Offset for Clock Gating Register 47
// #define CCM_TARGET_ROOT87_OFFSET    0xAB80  // Offset for Target Root Register
#define CCM_TARGET_ROOT87_OFFSET    0x00000B80  // Offset for Target Root Register

#define CCM_CCGR47_A53_VALUE    0x03 // Value to enable A53 clock in CCGR47

#define CCM_TARGET_ROOT87_ENABLE (1 << 28)
#define CCM_TARGET_ROOT87_CLOCK_SOURCE (1 << 24)


// Interrupt Enable Register Bit Masks
#define LUT_LOCK        (0x01)
#define LUT_UNLOCK      (0x02)

#define LUT_KEY_VALUE   (0x5AF05AF0) // Key value to unlock the LUT

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

#define LUT_TABLE_SIZE            (128) // Size of the LUT table
#define LUT_TABLE_OFFSET            (0x100)
#define LUT_INTEN_OFFSET            (0x10) // Offset for Interrupt Enable Register
#define LUT_MR0_OFFSET              (0x00) // Memory Register 0
#define LUT_KEY18_OFFSET            (0x18)    // Offset for LUT Key Register
#define LUT_CR_OFFSET               (0x1C)    // Offset for LUT Key Register
#define LUT_INTR_OFFSET             (0x14) // Interrupt Enable Register
#define LUT_IPCR0_OFFSET            (0xA0)
#define LUT_IPCR1_OFFSET            (0xA4)
#define LUT_IPTXFCR_OFFSET          (0xBC) // IP TX FIFO Control Register
#define LUT_IPRXFCR_OFFSET          (0xB8) // IP TX FIFO Control Register
#define LUT_IPCMD_OFFSET            (0xB0) // IP TX FIFO Control Register
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
    int init_done;                        // Initialization done flag
    uint32_t *QSPI_CCGR47;                // Clock Gating Register
    uint32_t *QSPI_TARGET_ROOT87;         // Target Root Register
    uint32_t *QSPI_INTEN;                 // Interrupt Enable Register
    uint32_t *QSPI_INTR;                  // Interrupt Register
    uint32_t *QSPI_MR0;                   // Memory Register 0
    uint32_t *QSPI_LUTKEY;                // LUT Key Register
    uint32_t *QSPI_LUTCR;                 // LUT Control Register
    uint32_t *QSPI_IPCR0;                 // IP Command Register 0
    uint32_t *QSPI_IPCR1;                 // IP Command Register 1
    uint32_t *QSPI_IPCMD;                 // IP Command Register
    uint32_t *QSPI_IPTXFCR;               // IP TX FIFO Control Register
    uint32_t *QSPI_IPRXFCR;               // IP TX FIFO Control Register
    uint32_t (*QSPI_LUT)[LUT_TABLE_SIZE]; // LUT base address for command sequences
} QSPI_Registers;

STATIC QSPI_Context qspi_ctx = {};
STATIC QSPI_Registers qspi_regs = {};

static int require_memory(QSPI_Context *ctx)
{
    // int flags = MAP_SHARED | MAP_FIXED; // Use MAP_SHARED to allow other processes to access the memory
    int flags = MAP_SHARED;
    int fd = ctx->fd;
    ctx->page_size = sysconf(_SC_PAGESIZE);
    // ctx->QSPI_CCGR_BASE = (uint32_t *)mmap((void *)CCM_CCGR_BASE, ctx->page_size, PROT_READ | PROT_WRITE, flags, fd, 0);
    ctx->QSPI_CCGR_BASE = (uint32_t *)mmap(NULL, ctx->page_size, PROT_READ | PROT_WRITE, flags, fd, CCM_CCGR_BASE);
    if (ctx->QSPI_CCGR_BASE == MAP_FAILED)
    {
        slogt("Failed to map QSPI CCGR base: %s", strerror(errno));
        return -1;
    }

    // ctx->QSPI_TARGET_BASE = (uint32_t *)mmap((void *)(CCM_TARGET_ROOT_BASE + CCM_CCGR47_OFFSET), ctx->page_size,
    //                                          PROT_READ | PROT_WRITE, flags, fd, 0);
    ctx->QSPI_TARGET_BASE = (uint32_t *)mmap(NULL, ctx->page_size,
                                             PROT_READ | PROT_WRITE, flags, fd, (CCM_TARGET_ROOT_BASE + CCM_CCGR47_OFFSET));
    if (ctx->QSPI_TARGET_BASE == MAP_FAILED)
    {
        slogt("Failed to map QSPI Target Root base: %s", strerror(errno));
        // Clean up previously mapped memory
        munmap(ctx->QSPI_CCGR_BASE, ctx->page_size);
        return -1;
    }

    // ctx->QSPI_LUT_BASE = (uint32_t *)mmap((void *)CCM_LUT_BASE, ctx->page_size, PROT_READ | PROT_WRITE, flags, fd, 0);
    ctx->QSPI_LUT_BASE = (uint32_t *)mmap(NULL, ctx->page_size, PROT_READ | PROT_WRITE, flags, fd, CCM_LUT_BASE);
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

STATIC void map_registers(QSPI_Registers *regs, QSPI_Context *ctx)
{
    regs->QSPI_CCGR47 = (uint32_t *)((uint32_t)ctx->QSPI_CCGR_BASE + CCM_CCGR47_OFFSET);
    regs->QSPI_TARGET_ROOT87 = (uint32_t *)((uint32_t)ctx->QSPI_TARGET_BASE | CCM_TARGET_ROOT87_OFFSET);
    regs->QSPI_INTEN = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_INTEN_OFFSET);
    regs->QSPI_INTR = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_INTR_OFFSET);
    regs->QSPI_MR0 = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_MR0_OFFSET);
    regs->QSPI_LUTKEY = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_KEY18_OFFSET);
    regs->QSPI_LUTCR = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_CR_OFFSET);
    regs->QSPI_IPCR0 = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_IPCR0_OFFSET);
    regs->QSPI_IPCR1 = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_IPCR1_OFFSET);
    regs->QSPI_IPCMD = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_IPCMD_OFFSET);
    regs->QSPI_IPTXFCR = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_IPTXFCR_OFFSET);
    regs->QSPI_IPRXFCR = (uint32_t *)((uint32_t)ctx->QSPI_LUT_BASE + LUT_IPRXFCR_OFFSET);
    regs->QSPI_LUT = (uint32_t (*)[LUT_TABLE_SIZE])((uint32_t)ctx->QSPI_LUT_BASE + LUT_TABLE_OFFSET);
    regs->init_done = 1; // Mark registers as initialized
}
void print_qspi_register_addresses(QSPI_Registers *regs)
{
    slogt("QSPI_CCGR47:         addr=0x%X", (void *)regs->QSPI_CCGR47);
    slogt("QSPI_TARGET_ROOT87:  addr=0x%X", (void *)regs->QSPI_TARGET_ROOT87);
    slogt("QSPI_INTEN:          addr=0x%X", (void *)regs->QSPI_INTEN);
    slogt("QSPI_INTR:           addr=0x%X", (void *)regs->QSPI_INTR);
    slogt("QSPI_MR0:            addr=0x%X", (void *)regs->QSPI_MR0);
    slogt("QSPI_LUTKEY:         addr=0x%X", (void *)regs->QSPI_LUTKEY);
    slogt("QSPI_LUTCR:          addr=0x%X", (void *)regs->QSPI_LUTCR);
    slogt("QSPI_IPCR0:          addr=0x%X", (void *)regs->QSPI_IPCR0);
    slogt("QSPI_IPCR1:          addr=0x%X", (void *)regs->QSPI_IPCR1);
    slogt("QSPI_IPCMD:          addr=0x%X", (void *)regs->QSPI_IPCMD);
    slogt("QSPI_IPTXFCR:        addr=0x%X", (void *)regs->QSPI_IPTXFCR);
    slogt("QSPI_IPRXFCR:        addr=0x%X", (void *)regs->QSPI_IPRXFCR);
    slogt("QSPI_LUT:            addr=0x%X", (void *)regs->QSPI_LUT);
}

void print_qspi_context(QSPI_Context *ctx)
{
    slogt("QSPI Context:");
    slogt("  fd: %d", ctx->fd);
    slogt("  init_done: %d", ctx->init_done);
    slogt("  page_size: %ld", ctx->page_size);
    slogt("  QSPI_CCGR_BASE: 0x%X", ctx->QSPI_CCGR_BASE);
    slogt("  QSPI_TARGET_BASE: 0x%X", ctx->QSPI_TARGET_BASE);
    slogt("  QSPI_LUT_BASE: 0x%X", ctx->QSPI_LUT_BASE);
}

static void unlock_lut(QSPI_Registers *regs)
{
    // Unlock LUT for update
    *regs->QSPI_LUTKEY = LUT_KEY_VALUE;
    *regs->QSPI_LUTCR = LUT_UNLOCK;
}

static void lock_lut(QSPI_Registers *regs)
{
    // Lock LUT after update
    *regs->QSPI_LUTKEY = LUT_KEY_VALUE;
    *regs->QSPI_LUTCR = LUT_LOCK;
}

static void update_lut(QSPI_Registers *regs, uint32_t index, const uint32_t *cmd, uint32_t count)
{
    int i;
    uint32_t *lutBase;
    assert(index < LUT_TABLE_SIZE && "LUT index out of bounds");

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

    // print the mapped addresses for debugging
    print_qspi_context(&qspi_ctx);
    // Check if initialization is already done
    assert(qspi_ctx.init_done == 1 && "QSPI context initialization failed");
    assert(qspi_ctx.QSPI_CCGR_BASE == (uint32_t *)(CCM_CCGR_BASE) && "QSPI CCGR base mapping failed");
    assert(qspi_ctx.QSPI_TARGET_BASE == (uint32_t *)(CCM_TARGET_ROOT_BASE) && "QSPI Target Root base mapping failed");
    assert(qspi_ctx.QSPI_LUT_BASE == (uint32_t *)CCM_LUT_BASE && "QSPI LUT base mapping failed");

    // Map the registers
    map_registers(&qspi_regs, &qspi_ctx);

    // print the mapped register addresses for debugging
    print_qspi_register_addresses(&qspi_regs);

    assert(qspi_regs.QSPI_CCGR47 == (void *)0x303842F0 && "QSPI CCGR47 register mapping failed");
    assert(qspi_regs.QSPI_TARGET_ROOT87 == (void *)0x3038AB80 && "QSPI CCGR47 register mapping failed");
    assert(qspi_regs.QSPI_INTEN == (void *)0x30BB0010 && "QSPI Interrupt Enable register mapping failed");
    assert(qspi_regs.QSPI_INTR == (void *)0x30BB0014 && "QSPI Interrupt register mapping failed");
    assert(qspi_regs.QSPI_MR0 == (void *)0x30BB0000 && "QSPI Memory Register 0 mapping failed");
    assert(qspi_regs.QSPI_LUTKEY == (void *)0x30BB0018 && "QSPI LUT Key register mapping failed");
    assert(qspi_regs.QSPI_LUTCR == (void *)0x30BB001C && "QSPI LUT Control register mapping failed");
    assert(qspi_regs.QSPI_IPCR0 == (void *)0x30BB00A0 && "QSPI IP Command Register 0 mapping failed");
    assert(qspi_regs.QSPI_IPCR1 == (void *)0x30BB00A4 && "QSPI IP Command Register 1 mapping failed");
    assert(qspi_regs.QSPI_IPCMD == (void *)0x30BB00B0 && "QSPI IP Command register mapping failed");
    assert(qspi_regs.QSPI_IPTXFCR == (void *)0x30BB00BC && "QSPI IP TX FIFO Control register mapping failed");
    assert(qspi_regs.QSPI_IPRXFCR == (void *)0x30BB00B8 && "QSPI IP TX FIFO Control register mapping failed");
    assert(qspi_regs.QSPI_LUT == (void *)(0x30BB0100) && "QSPI LUT base mapping failed");

    slogt("QSPI registers mapped successfully");

    *qspi_regs.QSPI_CCGR47 = CCM_CCGR47_A53_VALUE;
    slogt("QSPI clock enabled: 0x%08X:0x%08X", qspi_regs.QSPI_CCGR47, *qspi_regs.QSPI_CCGR47);

    /// Step 2: Set the clock root register CCM_TARGET_ROOT87 with address 0x3038AB80 to determine the flexspi clock
    /// frequency (Page 459). The CCM_TARGET_ROOT87[MUX] determines the clock source (Page 239). Please note that bit 28
    /// must be set to 1.

    *qspi_ctx.QSPI_TARGET_BASE = CCM_TARGET_ROOT87_ENABLE | CCM_TARGET_ROOT87_CLOCK_SOURCE;
    slogt("QSPI Target Root Register set: 0x%08X:0x%08X", *qspi_regs.QSPI_TARGET_ROOT87, *qspi_regs.QSPI_TARGET_ROOT87);
    //
    // /// Step 3: Set the associated bits in interrupt enable register with address 0x30BB0010 (Page 2458).

    *qspi_regs.QSPI_INTEN = ITEN_SEQTIMEOUTN | ITEN_AHBBUSTIMEOUTEN | ITEN_SCKSTOPBYWREN | ITEN_SCKSTOPBYRDEN |
                            ITEN_DATALEARNFAILEN | ITEN_IPTXWEEN | ITEN_IPRXWAEN | ITEN_AHBCMDERRREN |
                            ITEN_IPCMDERRREN | ITEN_AHBCMDGEEN | ITEN_IPCMDGEEN | ITEN_IPCMDDONEEN;

    slogt("QSPI Interrupt Enable Register set: 0x%08X:0x%08X", qspi_regs.QSPI_INTEN, *qspi_regs.QSPI_INTEN);

    /// Step 4: Enable the flexspi by setting MCR0[MDIS] to 0 (Page 2450).
    *qspi_regs.QSPI_MR0 &= ~(1 << 1); // Clear the MDIS bit to enable the module
    slogt("QSPI Memory Register 0 set: 0x%08X:0x%08X", qspi_regs.QSPI_MR0, *qspi_regs.QSPI_MR0);

    /// Step 5: Unlock Look Up Table (LUT). LUTKEY (0x30BB0018) and LUTCR (0x30BB001C).
    *qspi_regs.QSPI_LUTKEY = LUT_KEY_VALUE; // Write the key value to unlock
    *qspi_regs.QSPI_LUTCR = LUT_UNLOCK;     // Set the LUTCR to unlock
    slogt("QSPI LUT Lock state: 0x%08X:0x%08X", qspi_regs.QSPI_LUTCR, *qspi_regs.QSPI_LUTCR);

    slogi("QSPI initialization completed successfully");
}

int QSPI_IsInitialized()
{
    return qspi_ctx.init_done && qspi_regs.init_done;
}

void QSPI_DeInit()
{
    if (!qspi_ctx.init_done)
    {
        slogi("QSPI is not initialized, nothing to deinitialize");
        return;
    }

    // Clean up memory mappings
    if (qspi_ctx.QSPI_CCGR_BASE)
        munmap(qspi_ctx.QSPI_CCGR_BASE, qspi_ctx.page_size);
    if (qspi_ctx.QSPI_TARGET_BASE)
        munmap(qspi_ctx.QSPI_TARGET_BASE, qspi_ctx.page_size);
    if (qspi_ctx.QSPI_LUT_BASE)
        munmap(qspi_ctx.QSPI_LUT_BASE, qspi_ctx.page_size);

    // Close the file descriptor
    if (qspi_ctx.fd >= 0)
    {
        close(qspi_ctx.fd);
        slogt("Closed /dev/mem file descriptor");
    }

    qspi_ctx.init_done = 0;
    memset(&qspi_regs, 0, sizeof(QSPI_Registers));
    slogt("QSPI resources cleaned up");
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
const uint32_t customLUT[] = {
    /* 8bit mode */
    /*  read Quad mode - SDR */
    [4 * ARD_SEQ_INDEX] = FlexSPI_LUT_SEQ(LUT_CMD, kFlexSPI_4PAD, ARD_SEQ_CMD, LUT_ADDR, kFlexSPI_4PAD, 0x20),
    [4 * ARD_SEQ_INDEX + 1] = FlexSPI_LUT_SEQ(LUT_DUMMY, kFlexSPI_4PAD, 0x06, LUT_NXP_READ, kFlexSPI_4PAD, 0x04),
    [4 * ARD_SEQ_INDEX + 2] = FlexSPI_LUT_SEQ(LUT_DUMMY, kFlexSPI_4PAD, 0x02, LUT_STOP, kFlexSPI_4PAD, 0),

    /*  Write Quad mode - SDR */
    [4 * AWR_SEQ_INDEX] = FlexSPI_LUT_SEQ(LUT_CMD, kFlexSPI_4PAD, AWR_SEQ_CMD, LUT_ADDR, kFlexSPI_4PAD, 0x20),
    [4 * AWR_SEQ_INDEX + 1] = FlexSPI_LUT_SEQ(LUT_DUMMY, kFlexSPI_4PAD, 0x04, LUT_NXP_WRITE, kFlexSPI_4PAD, 0x04),
    [4 * AWR_SEQ_INDEX + 2] = FlexSPI_LUT_SEQ(LUT_DUMMY, kFlexSPI_4PAD, 0x02, LUT_STOP, kFlexSPI_4PAD, 0),

};

void QSPI_WriteBlocking(uint8_t *buffer, size_t size)
{
    uint32_t status;

    if (!QSPI_IsInitialized())
    {
        slogf("qspi is not initialized");
        return;
    }

    update_lut(&qspi_regs, 0, customLUT, 1);
}
