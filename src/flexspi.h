#ifndef FLEXSPI_H
#define FLEXSPI_H

#include <stdint.h>

#define FLASH_SIZE		(32*1024)	/* FPGA device size 32*1024(KB) = 32MB */
#define ARD_SEQ_NUMBER	1			/* Sequence number for AHB read command */
#define ARD_SEQ_INDEX	0			/* Sequence ID for AHB read command */

#define AWR_SEQ_NUMBER	1			/* Sequence number for AHB write command */
#define AWR_SEQ_INDEX   1			/* Sequence ID for AHB write command */

#define ARD_SEQ_CMD     0xBB        /* cmd for read */
#define AWR_SEQ_CMD     0xAA 	    /* cmd for write */




/* Instruction set for the LUT register */
#define LUT_STOP			0x00
#define LUT_CMD				0x01
#define LUT_ADDR			0x02
#define LUT_CADDR_SDR		0x03
#define LUT_MODE			0x04
#define LUT_MODE2			0x05
#define LUT_MODE4			0x06
#define LUT_MODE8			0x07
#define LUT_NXP_WRITE		0x08
#define LUT_NXP_READ		0x09
#define LUT_LEARN_SDR		0x0A
#define LUT_DATSZ_SDR		0x0B
#define LUT_DUMMY			0x0C
#define LUT_DUMMY_RWDS_SDR	0x0D
#define LUT_JMP_ON_CS		0x1F
#define LUT_CMD_DDR			0x21
#define LUT_ADDR_DDR		0x22
#define LUT_CADDR_DDR		0x23
#define LUT_MODE_DDR		0x24
#define LUT_MODE2_DDR		0x25
#define LUT_MODE4_DDR		0x26
#define LUT_MODE8_DDR		0x27
#define LUT_WRITE_DDR		0x28
#define LUT_READ_DDR		0x29
#define LUT_LEARN_DDR		0x2A
#define LUT_DATSZ_DDR		0x2B
#define LUT_DUMMY_DDR		0x2C
#define LUT_DUMMY_RWDS_DDR	0x2D

/*! @name LUT - LUT 0..LUT 127 */
#define FlexSPI_LUT_OPERAND0_MASK	(0xFFU)
#define FlexSPI_LUT_OPERAND0_SHIFT	(0U)
#define FlexSPI_LUT_OPERAND0(x)		(((uint32_t)(((uint32_t)(x)) << FlexSPI_LUT_OPERAND0_SHIFT)) & FlexSPI_LUT_OPERAND0_MASK)
#define FlexSPI_LUT_NUM_PADS0_MASK	(0x300U)
#define FlexSPI_LUT_NUM_PADS0_SHIFT	(8U)
#define FlexSPI_LUT_NUM_PADS0(x)	(((uint32_t)(((uint32_t)(x)) << FlexSPI_LUT_NUM_PADS0_SHIFT)) & FlexSPI_LUT_NUM_PADS0_MASK)
#define FlexSPI_LUT_OPCODE0_MASK	(0xFC00U)
#define FlexSPI_LUT_OPCODE0_SHIFT	(10U)
#define FlexSPI_LUT_OPCODE0(x)		(((uint32_t)(((uint32_t)(x)) << FlexSPI_LUT_OPCODE0_SHIFT)) & FlexSPI_LUT_OPCODE0_MASK)
#define FlexSPI_LUT_OPERAND1_MASK	(0xFF0000U)
#define FlexSPI_LUT_OPERAND1_SHIFT	(16U)
#define FlexSPI_LUT_OPERAND1(x)		(((uint32_t)(((uint32_t)(x)) << FlexSPI_LUT_OPERAND1_SHIFT)) & FlexSPI_LUT_OPERAND1_MASK)
#define FlexSPI_LUT_NUM_PADS1_MASK	(0x3000000U)
#define FlexSPI_LUT_NUM_PADS1_SHIFT	(24U)
#define FlexSPI_LUT_NUM_PADS1(x)	(((uint32_t)(((uint32_t)(x)) << FlexSPI_LUT_NUM_PADS1_SHIFT)) & FlexSPI_LUT_NUM_PADS1_MASK)
#define FlexSPI_LUT_OPCODE1_MASK	(0xFC000000U)
#define FlexSPI_LUT_OPCODE1_SHIFT	(26U)
#define FlexSPI_LUT_OPCODE1(x)		(((uint32_t)(((uint32_t)(x)) << FlexSPI_LUT_OPCODE1_SHIFT)) & FlexSPI_LUT_OPCODE1_MASK)

/* Formula to form FLEXSPI instructions in LUT table */
#define FlexSPI_LUT_SEQ(cmd0, pad0, op0, cmd1, pad1, op1)															   \
	(FlexSPI_LUT_OPERAND0(op0) | FlexSPI_LUT_NUM_PADS0(pad0) | FlexSPI_LUT_OPCODE0(cmd0) | FlexSPI_LUT_OPERAND1(op1) | \
	FlexSPI_LUT_NUM_PADS1(pad1) | FlexSPI_LUT_OPCODE1(cmd1))

/* FlexSPI AHB buffer count */
#define FSL_FEATURE_FlexSPI_AHB_BUFFER_COUNT	8

/* FlexSPI - Register Layout Typedef */
typedef struct {
	uint32_t MCR0;								/**< Module Control Register 0, offset: 0x0 */
	uint32_t MCR1;								/**< Module Control Register 1, offset: 0x4 */
	uint32_t MCR2;								/**< Module Control Register 2, offset: 0x8 */
	uint32_t AHBCR;								/**< AHB Bus Control Register, offset: 0xC */
	uint32_t INTEN;								/**< Interrupt Enable Register, offset: 0x10 */
	uint32_t INTR;								/**< Interrupt Register, offset: 0x14 */
	uint32_t LUTKEY;							/**< LUT Key Register, offset: 0x18 */
	uint32_t LUTCR;								/**< LUT Control Register, offset: 0x1C */
	uint32_t AHBRXBUFCR0[8];					/**< AHB RX Buffer 0 Control Register 0..AHB RX Buffer 7 Control Register 0, array offset: 0x20, array step: 0x4 */
	uint8_t RESERVED_0[32];
	uint32_t FLSHCR0[4];						/**< Flash Control Register 0, array offset: 0x60, array step: 0x4 */
	uint32_t FLSHCR1[4];						/**< Flash Control Register 1, array offset: 0x70, array step: 0x4 */
	uint32_t FLSHCR2[4];						/**< Flash Control Register 2, array offset: 0x80, array step: 0x4 */
	uint8_t RESERVED_1[4];
	uint32_t FLSHCR4;							/**< Flash Control Register 4, offset: 0x94 */
	uint8_t RESERVED_2[8];
	uint32_t IPCR0;								/**< IP Control Register 0, offset: 0xA0 */
	uint32_t IPCR1;								/**< IP Control Register 1, offset: 0xA4 */
	uint8_t RESERVED_3[8];
	uint32_t IPCMD;								/**< IP Command Register, offset: 0xB0 */
	uint32_t DLPR;								/**< Data Learn Pattern Register, offset: 0xB4 */
	uint32_t IPRXFCR;							/**< IP RX FIFO Control Register, offset: 0xB8 */
	uint32_t IPTXFCR;							/**< IP TX FIFO Control Register, offset: 0xBC */
	uint32_t DLLCR[2];							/**< DLL Control Register 0, array offset: 0xC0, array step: 0x4 */
	uint8_t RESERVED_4[24];
	uint32_t STS0;								/**< Status Register 0, offset: 0xE0 */
	uint32_t STS1;								/**< Status Register 1, offset: 0xE4 */
	uint32_t STS2;								/**< Status Register 2, offset: 0xE8 */
	uint32_t AHBSPNDSTS;						/**< AHB Suspend Status Register, offset: 0xEC */
	uint32_t IPRXFSTS;							/**< IP RX FIFO Status Register, offset: 0xF0 */
	uint32_t IPTXFSTS;							/**< IP TX FIFO Status Register, offset: 0xF4 */
	uint8_t RESERVED_5[8];
	uint32_t RFDR[32];							/**< IP RX FIFO Data Register 0..IP RX FIFO Data Register 31, array offset: 0x100, array step: 0x4 */
	uint32_t TFDR[32];							/**< IP TX FIFO Data Register 0..IP TX FIFO Data Register 31, array offset: 0x180, array step: 0x4 */
	uint32_t LUT[128];							/**< LUT 0..LUT 127, array offset: 0x200, array step: 0x4 */
} FlexSPI_Type;




/*! @brief FLEXSPI sample clock source selection for Flash Reading.*/
typedef enum _flexspi_read_sample_clock
{
	kFlexSPI_ReadSampleClkLoopbackInternally = 0x0U,	  /*!< Dummy Read strobe generated by FlexSPI Controller
															   and loopback internally. */
	kFlexSPI_ReadSampleClkLoopbackFromDqsPad = 0x1U,	  /*!< Dummy Read strobe generated by FlexSPI Controller
															   and loopback from DQS pad. */
	kFlexSPI_ReadSampleClkLoopbackFromSckPad	  = 0x2U, /*!< SCK output clock and loopback from SCK pad. */
	kFlexSPI_ReadSampleClkExternalInputFromDqsPad = 0x3U, /*!< Flash provided Read strobe and input from DQS pad. */
} flexspi_read_sample_clock_t;

typedef struct _flexspi_ahbBuffer_config
{
	uint8_t priority;	 /*!< This priority for AHB Master Read which this AHB RX Buffer is assigned. */
	uint8_t masterIndex; /*!< AHB Master ID the AHB RX Buffer is assigned. */
	uint16_t bufferSize; /*!< AHB buffer size in byte. */
	bool enablePrefetch; /*!< AHB Read Prefetch Enable for current AHB RX Buffer corresponding Master, allows
						  prefetch disable/enable separately for each master. */
} flexspi_ahbBuffer_config_t;

/*! @brief FLEXSPI interval unit for flash device select.*/
typedef enum _flexspi_cs_interval_cycle_unit
{
	kFlexSPI_CsIntervalUnit1SckCycle   = 0x0U, /*!< Chip selection interval: CSINTERVAL * 1 serial clock cycle. */
	kFlexSPI_CsIntervalUnit256SckCycle = 0x1U, /*!< Chip selection interval: CSINTERVAL * 256 serial clock cycle. */
} flexspi_cs_interval_cycle_unit_t;

/*! @brief FLEXSPI AHB wait interval unit for writing.*/
typedef enum _flexspi_ahb_write_wait_unit
{
	kFlexSPI_AhbWriteWaitUnit2AhbCycle	   = 0x0U, /*!< AWRWAIT unit is 2 ahb clock cycle. */
	kFlexSPI_AhbWriteWaitUnit8AhbCycle	   = 0x1U, /*!< AWRWAIT unit is 8 ahb clock cycle. */
	kFlexSPI_AhbWriteWaitUnit32AhbCycle    = 0x2U, /*!< AWRWAIT unit is 32 ahb clock cycle. */
	kFlexSPI_AhbWriteWaitUnit128AhbCycle   = 0x3U, /*!< AWRWAIT unit is 128 ahb clock cycle. */
	kFlexSPI_AhbWriteWaitUnit512AhbCycle   = 0x4U, /*!< AWRWAIT unit is 512 ahb clock cycle. */
	kFlexSPI_AhbWriteWaitUnit2048AhbCycle  = 0x5U, /*!< AWRWAIT unit is 2048 ahb clock cycle. */
	kFlexSPI_AhbWriteWaitUnit8192AhbCycle  = 0x6U, /*!< AWRWAIT unit is 8192 ahb clock cycle. */
	kFlexSPI_AhbWriteWaitUnit32768AhbCycle = 0x7U, /*!< AWRWAIT unit is 32768 ahb clock cycle. */
} flexspi_ahb_write_wait_unit_t;

/*! @brief FLEXSPI operation port select.*/
typedef enum _flexspi_port
{
	kFlexSPI_PortA1 = 0x0U, /*!< Access flash on A1 port. */
	kFlexSPI_PortA2,		/*!< Access flash on A2 port. */
	kFlexSPI_PortB1,		/*!< Access flash on B1 port. */
	kFlexSPI_PortB2,		/*!< Access flash on B2 port. */
	kFlexSPI_PortCount
} flexspi_port_t;

/*! @brief pad definition of FLEXSPI, use to form LUT instruction. */
typedef enum _flexspi_pad
{
	kFlexSPI_1PAD = 0x00U, /*!< Transmit command/address and transmit/receive data only through DATA0/DATA1. */
	kFlexSPI_2PAD = 0x01U, /*!< Transmit command/address and transmit/receive data only through DATA[1:0]. */
	kFlexSPI_4PAD = 0x02U, /*!< Transmit command/address and transmit/receive data only through DATA[3:0]. */
	kFlexSPI_8PAD = 0x03U, /*!< Transmit command/address and transmit/receive data only through DATA[7:0]. */
} flexspi_pad_t;




#endif /* FLEXSPI_H */
