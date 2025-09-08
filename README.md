# How to bold and run the tests 

- Build the docker image:
```bash
docker build -t qspitool .
```
- Run the docker image:
```bash
docker run -it --rm -v $(pwd):/workspace qspitool
```
- Run the tests:
```bash
make test
```
- Run the tests with Valgrind:
```bash
make test-valgrind
```



# QSPI configuration

qspi flash has a dedicated region in memory map with 256MB size.

QSPI Flash in CM7: 0xC000_0000-0xCFFF_FFFF 
QSPI Flash in CA53: 0x0800_0000-0x17FF_FFFF 

According to IMX8MPRM.pdf it is better to use the AHB bus to communicate with FPGA (P. 2447).

## Initialization

The main registers which should be initialized are the clock gate, interrupt enable and clock controller. By default, the clock gate for flexspi is not enabled.
1. Set the clock gate register CCM_CCGR47 with address 0x303842F0 to 0x30 in CM7 and to 0x3 in CA53.
1. Set the clock root register CCM_TARGET_ROOT87 with address 0x3038AB80 to determine the flexspi clock frequency (Page 459). 
    The CCM_TARGET_ROOT87[MUX] determines the clock source (Page 239).
    Please note that bit 28 must be set to 1.
1. Set the associated bits in interrupt enable register with address 0x30BB0010 (Page 2458).
1. Enable the flexspi by setting MCR0[MDIS] to 0 (Page 2450).
1.  Unlock Look Up Table (LUT). LUTKEY (0x30BB0018) and LUTCR (0x30BB001C).

## Send data IP command

To send data through IP command, the data should be pushed to TX_IP_FIFO then send command should be executed.
1. Determine which look up table should be execute. LUT instructions should be stored in address 0x30BB0200~0x30BB3FC. 
1. Set the start address in register IPCR0 with the address 0x30BB00A0.
1. Write data to IP_TX_FIFO.
1. Triger the INTR[IPTXWE] bit. Without this command, the data will not be pushed to the IP_TX_FIFO.
1. The size of data written to IP_TX_FIFO should be equal to (IPTXFCR[TXWMRK] * 64bits).
1. Determine sequence number and sequence index in register IPCR1 with address 0x30BB00A4. It is possible to determine data size in WRITE_CMD operand and if it sets to zero then the data size is determined by IPCR1.
1. Triger the command bit in IPCMD register with address 0x30BB00B0 (Page 2481).
1. If the send command is done with no issue the INTR[IPCMDDONE] bit will be set to 1. 
1. Triger clear tx buffer bit IPTXFCR[CLRIPTXF] with address 0x30BB00BC to update IP_TX_FIFO pointer.

## Receive data IP response

In some cases, the IP command should wait for the response.
1. Read command should be defined in LUT. And the size of data is stored in the operand of READ_CMD
2. It is also possible to enable the receive data interrupt.
3. Fetch data from IP_RX_FIFO.
4. Triger clear rx buffer bit IPRXFCR[CLRIPRXF] to update IP_RX_FIFO pointer.

## Interrupt(not in use for now)

This test is related to interrupt of FlexSpi in IMX8MP and it is implemented on CM7 with ZephyrOS. The steps are:
1. Setting the IRQ number of FlexSpi node in device tree. According to IMX8MPRM.pdf (p. 1000) the IRQ is 107.
1. Retrieving the FlexSPI node properties in device driver.
1. Enabling the FlexSPI IRQ and defining an IRQ handler in device driver.
1. Enabling an interrupt. List of interrupts are in IMX8MPRM.pdf (p. 2458).

It is worth noting that in ZephyrOS it is recommended to enable the interrupts in kernel side and in compilation then read or write data in application side.

IMX8- Master
FPGA - Slave

Syrius FPGA comunnication classes:
 - SyFPGAAuto - map8001
 - SyFPGAManSlot - map8200

Check Interrupts

10.2.2.10.2
Filling Data to IP TX FIFO

10.2.2.7.1
Instruction execution on SPI interface
