
# cat bios

BBC Basic for Cerberus currently requires a modified version of the standard BIOS to run.

The main differences are:

- A handful of compile-time configurations at top of source code
- Serial keyboard input over FTDI
- Data upload over Serial
- 50hz NMI interrupt enabled when CPU is running
- Boots with Z80 set at 8mhz as default
- Two-way comms between CAT and CPUs
- Tweaks to memory map
	- 0x0200: Outgoing mailbox flag (to CPU)
	- 0x0201: Outgoing mailbox data (to CPU)
	- 0x0202: Incoming mailbox flag (from CPU)
	- 0x0203: Incoming mailbox data (from CPU - word)
	- 0x0205: Code start

### Data upload over Serial

There is a sample Powershell script in the [Powershell](Powershell) folder that will upload bin files directly into the Cerberus RAM.

### Prerequisites

As per Alan Toone's 0xFE BIOS, this requires the slightly modified version of Paul Stoffregen's PS2 Keyboard library.

- Download the library from here as a ZIP file https://github.com/PaulStoffregen/PS2Keyboard
- Install the library in the Arduino IDE
- Locate the file PS2Keyboard.h
	- In Windows this may be in Documents/Arduino/libraries/PS2Keyboard-master
- Edit the #define for PS2_F12 (~ line 65)
	- `#define PS2_F12 1 // Change from 0 to 1`
- Compile the sketch and upload to the ATmega328p via an FTDI cable

### Compiling

The BIOS can be built as per the standard Cerberus BIOS. Pre-built version of the latest BIOS has been included as `.hex` files, with or without bootloader, and can be uploaded to the CAT with an Arduino hex loader.
