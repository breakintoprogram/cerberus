
# cat bios

This version of the Cerberus CAT code has the following features:

- Compatible with Cerberus 2080 and Cerberus 2100 with a compile-time switch
- A handful of compile-time configurations in config.h
- Serial keyboard input over FTDI
- Data upload over serial
- 50hz NMI interrupt enabled when CPU is running
- The selected CPU and FAST mode are saved in EEPROM
- Two-way comms between CAT and CPUs
- Tweaks to memory map
	- 0x0200: Outgoing mailbox flag (to CPU)
	- 0x0201: Outgoing mailbox data (to CPU)
	- 0x0202: Incoming mailbox flag (from CPU)
	- 0x0203: Incoming mailbox data (from CPU - word)
	- 0x0205: Code start
	- 0xEFFE: Expansion bus data (Cerberus 2100 only)
	- 0xEFFF: Expansion bus flag (Cerberus 2100 only)

### Data upload over Serial

There is a sample Powershell script in the [Powershell](../powershell) folder that will upload bin files directly into the Cerberus RAM.

### Prerequisites

This code requires a slightly modified version of Paul Stoffregen's PS2 Keyboard library, which is included in the source for your convenience within the src subfolder.

### Compiling

The BIOS can be built as per the standard Cerberus BIOS.

To compile for the Cerberus 2080, change the following line in config.h

```
#define config_board 1					// 0: Compile for Cerberus 2080, 1: Compile for Cerberus 2100
```
