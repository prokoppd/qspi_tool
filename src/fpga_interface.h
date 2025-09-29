#ifndef FPGA_INTERFACE_H
#define FPGA_INTERFACE_H

#include "flexspi.h"
#include "utils.h"

#define MAX_AN_CH 8

#define FPGA_TABLE(ENTRY)                                                      \
  /*    NAME     OPCODE */                                                     \
  ENTRY(WR_SPI1, 0x01)                                                         \
  ENTRY(WR_SPI2, 0x02)                                                         \
  ENTRY(WR_DCU_OUT, 0x03)                                                      \
  ENTRY(WR_GENERIC_CMD, 0x04)                                                  \
  ENTRY(WR_UART1, 0x05)                                                        \
  ENTRY(WR_UART2, 0x06)                                                        \
  ENTRY(WR_UART3, 0x07)                                                        \
  ENTRY(WR_UART4, 0x08)                                                        \
  ENTRY(WR_MCASP_CFG, 0x09)                                                    \
  ENTRY(WR_PPS_SEL, 0x0A)                                                      \
  ENTRY(WR_MST_CLK, 0x0C)                                                      \
  ENTRY(RD_SAMPLE, 0x80)                                                       \
  ENTRY(RD_SPI1, 0x81)                                                         \
  ENTRY(RD_SPI2, 0x82)                                                         \
  ENTRY(RD_UART1, 0x85)                                                        \
  ENTRY(RD_UART2, 0x86)                                                        \
  ENTRY(RD_UART3, 0x87)                                                        \
  ENTRY(RD_UART4, 0x88)                                                        \
  ENTRY(RD_SYNC_IN, 0x8B)                                                      \
  ENTRY(RD_MST_CLK, 0x8C)

#define EXPAND_AS_ENUM_VALUE(NAME, OPCODE) FPGA_OPCODE_##NAME = OPCODE,
#define EXPAND_AS_ENUM_INDEX(NAME, OPCODE) FPGA_LUT_IDX_##NAME,

typedef enum { FPGA_TABLE(EXPAND_AS_ENUM_VALUE) } fpga_opcode_t;

typedef enum {
  FPGA_TABLE(EXPAND_AS_ENUM_INDEX) FPGA_OPCODE_IDX_COUNT
} fpga_opcode_index_t;

typedef struct {
  uint32_t currIdx;
  uint32_t dummy1[4];
  int32_t smp[MAX_AN_CH];
  uint32_t extQuality;
  uint32_t outOfRange;
  uint32_t overflow;
  uint32_t hw_fail;
  uint16_t smpCount;
  uint8_t smpSyncH;
  uint8_t smpSyncHTrig;
  uint32_t dummy2;
} fpga_sample_t;

// clang-format off
extern const uint32_t fpga_lut[FPGA_OPCODE_IDX_COUNT * 4];
// clang-format on

#endif // FPGA_INTERFACE_H
