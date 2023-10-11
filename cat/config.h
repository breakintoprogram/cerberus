#ifndef CONFIG_H
#define CONFIG_H

// Compilation defaults
//
#define config_board 1					// 0: Compile for Cerberus 2080, 1: Compile for Cerberus 2100
#define	config_dev_mode	0				// Turn off various BIOS outputs to speed up development, specifically uploading code
#define config_silent 0					// Turn off the startup jingle
#define config_enable_nmi 1				// Turn on the 50hz NMI timer when CPU is running. If set to 0 will only trigger an NMI on keypress
#define config_default_mode 1			// 0: 6502, 1: Z80
#define config_default_fast 1			// 0: 4mhz, 1: 8mhz

#define	sys_vars 0xEFF0					// Base of system variables shared between CAT and CPUs

#define ptr_outbox_flag (sys_vars+0x0)	// Outbox flag memory location (byte)
#define ptr_outbox_data (sys_vars+0x1)	// Outbox data memory location (byte)
#define ptr_inbox_flag (sys_vars+0x2)	// Inbox flag memory location (byte)
#define	ptr_inbox_data (sys_vars+0x3)	// Inbox data memory location (word)
#define ptr_xbus_data (sys_vars+0xE)	// Expansion bus data memory location (byte)
#define ptr_xbus_flag (sys_vars+0xF)	// Expansion bus flag memory location (byte)

#define ptr_code_start 0x0200			// Default start location of code

#define config_eeprom_address_mode 0	// First EEPROM location
#define config_eeprom_address_fast 1	// Second EEPROM location

// Arduino ATMega328 Aliases
//
// NB: MISO, MOSI and SCK for SD Card are hardwired in CAT:
// CLK  -> pin 19 on CAT
// MISO -> pin 18 on CAT
// MOSI -> pin 17 on CAT
//
#define SI A5							// Serial Input, pin 28 on CAT
#define SO A4							// Serial Output, pin 27 on CAT
#define SC A3							// Shift Clock, pin 26 on CAT 
#define AOE A2							// Address Output Enable, pin 25 on CAT
#define RW A1							// Memory Read/!Write, pin 24 on CAT
#define LD A0							// Latch Data, pin 23 on CAT
#define CPUSLC 5						// CPU SeLeCt, pin 11 on CAT
#define CPUIRQ 6						// CPU Interrupt ReQuest, pin 12 on CAT
#define CPUGO 7							// CPU Go/!Halt, pin 13 on CAT
#define CPURST 8						// CPU ReSeT, pin 14 on CAT
#define CPUSPD 9						// CPU SPeeD, pin 15 on CAT
#define KCLK 2							// CLK pin connected to PS/2 keyboard (CAT pin 4)
#define KDAT 3							// DATA pin connected to PS/2 keyboard (CAT pin 5)
#define SOUND 4							// Sound output to buzzer, pin 6 on CAT
#define CS 10							// Chip Select for SD Card, pin 16 on CAT
#if config_board == 1
#define FREE 25							// Bit 2 of CPU CLocK Speed, pin 19 on FAT-CAT
#define XBUSACK 23						// eXpansion BUS ACKnowledgment, pin 3 on FAT-CAT, active LOW
#define XBUSREQ 24						// eXpansion BUS REQuest, pin 6 on FAT-CAT, active LOW
#define XIRQ 26							// eXpansion Interrupt ReQuest, pin 22 on FAT-CAT, active LOW
#endif

#endif // CONFIG_H