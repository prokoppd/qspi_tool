# QSPI configuration

qspi flash has a dedicated region in memory map with 256MB size.

QSPI Flash in CM7: 0xC000_0000-0xCFFF_FFFF 
QSPI Flash in CA53: 0x0800_0000-0x17FF_FFFF 

According to IMX8MPRM.pdf it is better to use the AHB bus to communicate with FPGA (P. 2447).

## Initialization

CCM_CCGR47 3038_42F0


The main registers which should be initialized are the clock gate, interrupt enable and clock controller. By default, the clock gate for flexspi is not enabled.
- Set the clock gate register CCM_CCGR47 with address 0x303842F0 to 0x30 in CM7 and to 0x3 in CA53.
- Set the clock root register CCM_TARGET_ROOT87 with address 0x3038AB80 to determine the flexspi clock frequency (Page 459). The CCM_TARGET_ROOT87[MUX] determines the clock source (Page 239). Please note that bit 28 must be set to 1.
- Set the associated bits in interrupt enable register with address 0x30BB0010 (Page 2458).
- Enable the flexspi by setting MCR0[MDIS] to 0 (Page 2450).
-  Unlock Look Up Table (LUT). LUTKEY (0x30BB0018) and LUTCR (0x30BB001C).
