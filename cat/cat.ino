//
// Title:			Cerberus 2080/2100 CAT BIOS code
// Author:			Bernado Kastrup aka The Byte Attic
// Copyright:		2020-2023
// Updated By:		Dean Belfield aka Break Into Program
// Contributors:	Aleksandr Sharikhin
// Created:			31/07/2021		
// Last Updated:	09/10/2023 		
//
// For the ATmega328p-PU (CAT) to be compiled in the arduino IDE   
// All rights reserved, code distributed under license     
//
// Provided AS-IS, without guarantees of any kind                        
// This is NOT a commercial product as has not been exhaustively tested  
//
// The directive "F()", which tells the compiler to put strings in code memory 
// instead of dynamic memory, is used as often as possible to save dynamic     
// memory, the latter being the bottleneck of this code.   
//
// How to compile:
//  - Use minicore library(https://github.com/MCUdude/MiniCore)
//  - Select board ATmega328
//  - Clock: External 16MHz
//  - BOD: 4.3V
//  - Varian: 328PB

// Modinfo:
// 10/08/2021: All recognised keystrokes sent to mailbox, F12 now returns to BIOS, 50hz NMI starts when code runs
// 11/08/2021: Memory map now defined in configs, moved code start to 0x0205 to accomodate inbox
// 12/08/2021: Refactored load, save and delFile. Now handles incoming messages from Z80/6502
// 21/08/2021: Tweaks for sound command, bug fixes in incoming message handler
// 06/10/2021: Tweaks for cat command
// 23/11/2021: Moved PS2Keyboard library from Arduino library to src subdirectory
// 22/07/2023: Working on Cerberus 2100 (Aleksandr Sharikhin)
//             PS2 keyboard resets on start(more keyboards will work). 
//             Optimizing loading splash screen. 
//             Removed unused keymaps. 
//             One command basic loading. Non blocking sound API.
//             Updated load command - returning how many bytes was actually read
// 27/08/2023: EEPROM-based persistent settings for mode and CPU speed (Aleksandr Sharikhin)
// 09/10/2023: Merged with Cerberus 2080 code with conditional compilation
//

// These libraries are built into the arduino IDE 
//
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>

// These need to be installed in the library manager
//
#include <TimerOne.h> 					// v1.1.1

// For more information about this PS2Keyboard library:   
// http://www.arduino.cc/playground/Main/PS2Keyboard      
// http://www.pjrc.com/teensy/td_libs_PS2Keyboard.html    
// Note that the Arduino managed library is not the latest
// Install from the GitHub project in order to build      

#include "src/PS2Keyboard/PS2Keyboard.h"

// Status constants
//
#define STATUS_DEFAULT 0
#define STATUS_BOOT 1
#define STATUS_READY 2
#define STATUS_UNKNOWN_COMMAND 3
#define STATUS_NO_FILE 4
#define STATUS_CANNOT_OPEN 5  
#define STATUS_MISSING_OPERAND 6
#define STATUS_SCROLL_PROMPT 7
#define STATUS_FILE_EXISTS 8
#define STATUS_ADDRESS_ERROR 9
#define STATUS_POWER 10
#define STATUS_EOF 11

// And all the internal includes
//
#include "config.h"
#include "hardware.h"
#include "fileio.h"

// Now some stuff required by the libraries in use
//
const int	chipSelect = CS;
const int	DataPin = KDAT;
const int	IRQpin = KCLK;

const char * bannerFilename = "icon2080.img";
const char * helpFilename = "help.txt";

char editLine[38];							// Current edit line buffer
char previousEditLine[38];					// Previous edit line buffer (allows for repeating previous command)

volatile byte pos = 1;						// Position in edit line currently occupied by cursor 

File		cd;								// Used by BASIC directory commands
FileIO		fileIO;							// All the FileIO stuff is in here
PS2Keyboard	keyboard;
Hardware	cerberus(config_board);

// Interrupt service routine attached to pin XIRQ (Cerberus 2100 only)
//
#if config_board == 1
ISR(PCINT3_vect) {
  cerberus.expFlag = true;
}
#endif

// CPU Interrupt Routine (50hz)
//
void cpuInterrupt(void) {
  	if(cerberus.cpuRunning) {							// Only run this code if cpu is running 
	   	digitalWrite(CPUIRQ, HIGH);		 				// Trigger an NMI interrupt
	   	digitalWrite(CPUIRQ, LOW);
  	}
	cerberus.interruptFlag = true;
}

void setup() {
	cerberus.initialise();				// Initialise the board
  	cerberus.resetCPU();				// Reset the CPUs
  	clearEditLine();					// Clear the edit line buffers
  	storePreviousLine();
  	Serial.begin(115200);				// Initialise the serial library
  	keyboard.begin(DataPin, IRQpin);	// Initialise the keyboard library
	keyboard.send(0xFF); 				// Reset the keyboard

  	// Now access uSD card and load character definitions so we can put something on the screen
	//
  	if (!SD.begin(chipSelect)) {
    	// SD Card has either failed or is not present
    	// Since the character definitions thus can't be uploaded, accuse error with repeated tone and hang
    	while(true) {
      		beep();
      		delay(500);
    	}
  	}
  	// Load character defs into memory
	//
  	if(fileIO.loadFile("chardefs.bin", 0xf000) != STATUS_READY) {
		beep();
	}

  	// Now prepare the screen 
	//
  	cerberus.cls();
  	cprintFrames();
	cprintBanner();

  	cprintStatus(STATUS_BOOT);
  	#if config_silent == 0
  	playJingle();							// Play a little jingle whilst the keyboard finishes initialising
  	#endif
  	delay(1000);
  	cprintStatus(STATUS_DEFAULT);
  	cprintEditLine();
}

char readKey() {
	char ascii = 0;
	if(keyboard.available()) {
		ascii = keyboard.read();
    	tone(SOUND, 750, 5);            	// Clicking sound for auditive feedback to key presses 
		#if config_dev_mode == 0        	
  		if(!cerberus.cpuRunning) {
			cprintStatus(STATUS_DEFAULT);	// Update status bar
		}
		#endif 
	}
	else if(Serial.available()) {
		ascii = Serial.read();
	}
	return ascii;
}

// The main loop
//
void loop() {
	char ascii = readKey();	// Stores ascii value of key pressed 
	byte i;     			// Just a counter 

  	// Wait for a key to be pressed, then take it from there... 
	//
	if(ascii > 0) {
    	if(cerberus.cpuRunning) {
    		if (ascii == PS2_F12) {  								// This happens if F12 has been pressed... and so on...
	    		stopCode();
    		}
    		else {
				cerberus.cpuRunning = false;						// Just stops interrupts from happening 
        		digitalWrite(CPUGO, LOW);   						// Pause the CPU and tristate its buses to high-Z 
      			byte mode = cerberus.peek(ptr_outbox_flag);
      			cerberus.poke(ptr_outbox_data, byte(ascii));		// Put token code of pressed key in the CPU's mailbox, at ptr_outbox_data
	         	cerberus.poke(ptr_outbox_flag, byte(0x01));			// Flag that there is new mail for the CPU waiting at the mailbox
      			digitalWrite(CPUGO, HIGH);  						// Let the CPU go
				cerberus.cpuRunning = true;
      			#if config_enable_nmi == 0
      			digitalWrite(CPUIRQ, HIGH); 						// Trigger an interrupt 
      			digitalWrite(CPUIRQ, LOW);
      			#endif
    		}
    	}
    	else {
	 	   switch(ascii) {
	    		case PS2_ENTER:
		    		exec(&editLine[1]);					// The first character is a ">"
	    			break;
	    		case PS2_UPARROW:
					pos = 0;
		     	   	for (i = 0; i < 38; i++) editLine[i] = previousEditLine[i];
        			cprintEditLine();
        			break;
        		case PS2_DOWNARROW:
		        	clearEditLine();
        			break;
        		case PS2_DELETE:
        		case PS2_LEFTARROW:
			        editLine[pos] = 32;					// Put an empty space in current cursor position
        			if (pos > 1) pos--;					// Update cursor position, unless reached left-most position already
        			editLine[pos] = 0;					// Put cursor on updated position
        			cprintEditLine();					// Print the updated edit line
        			break;
        		default:
		        	editLine[pos] = ascii;				// Put new character in current cursor position
        			if (pos < 37) pos++;				// Update cursor position
        			editLine[pos] = 0;					// Place cursor to the right of new character
    				#if config_dev_mode == 0			        	
		       		cprintEditLine();					// Print the updated edit line 
	       			#endif
        			break;        
			}
		}    
	}
	if(cerberus.interruptFlag) {						// If the interrupt flag is set then
		cerberus.interruptFlag = false;
		messageHandler();								// Run the inbox message handler
	}
	cerberus.xbusHandler();
}

// Inbox message handler
//
void messageHandler(void) {
  	int	flag, status;
  	byte retVal = 0x00;									// Return status; default is OK
  	unsigned int address;								// Pointer for data

 	if(cerberus.cpuRunning) {							// Only run this code if cpu is running 
	 	cerberus.cpuRunning = false;					// Just to prevent interrupts from happening
		digitalWrite(CPUGO, LOW); 						// Pause the CPU and tristate its buses to high-Z
		flag = cerberus.peek(ptr_inbox_flag);			// Fetch the inbox flag 
		if(flag > 0 && flag < 0x80) {
			address = cerberus.peekWord(ptr_inbox_data);
			switch(flag) {
				case 0x01:
					cmdSound(address, true);
					break;
				case 0x02: 
					status = cmdLoad(address);
					if(status != STATUS_READY) {
						retVal = (byte)(status + 0x80);
					}
					break;
				case 0x03:
					status = cmdSave(address);
					if(status != STATUS_READY) {
						retVal = (byte)(status + 0x80);
					}
					break;
				case 0x04:
					status = cmdDelFile(address);
					if(status != STATUS_READY) {
						retVal = (byte)(status + 0x80);
					}
					break;
				case 0x05:
					status = cmdCatOpen(address);
					if(status != STATUS_READY) {
						retVal = (byte)(status + 0x80);
					}
					break;
				case 0x06:
					status = cmdCatEntry(address);
					if(status != STATUS_READY) {
						retVal = (byte)(status + 0x80);
					}
					break;
        		case 0x7E:
          			cmdSound(address, false);
          			status = STATUS_READY;
          			break;
				case 0x7F:
					cerberus.reset();
					break;
			}
			cerberus.poke(ptr_inbox_flag, retVal);		// Flag we're done - values >= 0x80 are error codes
		}
		digitalWrite(CPUGO, HIGH);   					// Restart the CPU 
		cerberus.cpuRunning = true;
 	}
}

// Handle SOUND command from BASIC
//
void cmdSound(unsigned int address, bool blocking) {
	unsigned int frequency = cerberus.peekWord(address);
	unsigned int duration = cerberus.peekWord(address + 2) * 50;
	tone(SOUND, frequency, duration);
	if(blocking) {
		delay(duration);
	}
}

// Handle ERASE command from BASIC
//
int cmdDelFile(unsigned int address) {
	cerberus.peekString(address, (byte *)editLine, 38);
	return fileIO.deleteFile((char *)editLine);	
}

// Handle LOAD command from BASIC
//
int cmdLoad(unsigned int address) {
	unsigned int startAddr = cerberus.peekWord(address);
	unsigned int length = cerberus.peekWord(address + 2);
	cerberus.peekString(address + 4, (byte *)editLine, 38);
	return fileIO.loadFile((char *)editLine, startAddr);
}

// Handle SAVE command from BASIC
//
int cmdSave(unsigned int address) {
	unsigned int startAddr = cerberus.peekWord(address);
	unsigned int length = cerberus.peekWord(address + 2);
	cerberus.peekString(address + 4, (byte *)editLine, 38);
	return fileIO.saveFile((char *)editLine, startAddr, startAddr + length - 1);
}

// Handle CAT command from BASIC
//
int cmdCatOpen(unsigned int address) {
	cd = SD.open("/");								// Start the process first by opening the directory
	return STATUS_READY;
}

int cmdCatEntry(unsigned int address) {				// Subsequent calls to this will read the directory entries
	File entry;
	entry = cd.openNextFile();						// Open the next file
	if(!entry) {									// If we've read past the last file in the directory
		cd.close();									// Then close the directory
		return STATUS_EOF;							// And return end of file
	}
	cerberus.poke(address, entry.size());			// First four bytes are the length
	cerberus.poke(address + 4, entry.name());		// Followed by the filename, zero terminated
	entry.close();									// Close the directory entry
	return STATUS_READY;							// Return READY
}

// Generic beep
//
void beep(void) {
	tone(SOUND, 50, 150);
}

// Get the next word from the command line
//
String getNextParam() {
	return String(strtok(NULL, " "));
}

// Convert a hex string to a number
//
unsigned int fromHex(String s) {
	return strtol(s.c_str(), NULL, 16);
}

// Execute a command
// Parameters:
// - buffer: Pointer to a zero terminated string that contains the command with arguments
// Returns:
//
void exec(char * buffer) {
	String			param, response;
	unsigned int	addr;
	byte 			data, chkA, chkB;

	char *			ptr = buffer;

	ptr = strtok(buffer, " ");								// Fetch the command (first parameter)
	if(ptr != NULL) {
		param = String(ptr);								// Convert to a lower-case string
		param.toLowerCase();

		if(param.startsWith("0x")) {
			chkA = 1;
			chkB = 0;
			param.remove(0, 2);								// Strip the leading '0x' from the address
			response = param;								// Start of the response
			addr = fromHex(param);							// Convert the address string to an unsigned int
			param = getNextParam();							// Get the next byte
			while(param != "") {							// For as long as the user as entered bytes
				if(param.charAt(0) != '#') {				// Check for the terminator
					data = fromHex(param);					// Convert the byte string to a byte
					while(cerberus.peek(addr) != data) {	// Retry until written; serial comms may cause writes to be missed?
						cerberus.poke(addr, data);
					}
					chkA += data;							// Checksums
					chkB += chkA;
					addr ++;
				}
				else {
					param.remove(0,1);						// Check the checksum
					addr = fromHex(param);
					if( addr != ((chkA << 8) | chkB) ) {
						cprintString(28, 26, param);
						tone(SOUND, 50, 50);
					}
				}
				param = getNextParam();						// Fetch the next byte and repeat
			}
    		#if config_dev_mode == 0
    		cprintStatus(STATUS_READY);
    		cprintString(28, 27, response);
    		#endif
    		Serial.print(response);
    		Serial.print(' ');
    		Serial.println((unsigned int)((chkA << 8) | chkB), HEX);			
		}
  		else if (param == F("cls")) { 
    		cls();
    		cprintStatus(STATUS_READY);
  		}
		else if (param == F("6502")) { 
			cerberus.setMode(0);
    		cprintStatus(STATUS_READY);
		}
		else if (param == F("z80")) {
			cerberus.setMode(1);
    		cprintStatus(STATUS_READY);
		}
		else if (param == F("reset")) {
    		cerberus.reset();				    	
		}
		else if (param == F("fast")) {     	
			cerberus.setFast(1);
			cprintStatus(STATUS_READY);
		}
		else if (param == F("slow")) {     	
			cerberus.setFast(0);
    		cprintStatus(STATUS_READY);
  		} 
  		else if (param == F("run")) {
			storePreviousLine();
    		runCode();
		}
		else if (param == F("testmem")) {
    		cls();
    		cerberus.testMemory();
    		cprintStatus(STATUS_READY);
		}
		else if (param == F("list")) {
		    cls();
    		list(getNextParam());
    		cprintStatus(STATUS_READY);
		}
		else if (param == F("move")) {
			String startAddress = getNextParam();
			String endAddress = getNextParam();
			String destAddress = getNextParam();
    		binMove(startAddress, endAddress, destAddress);		
		}
		else if (param == F("basic6502")) {	
    		cerberus.mode = false;
			cerberus.cls();
    		digitalWrite(CPUSLC, LOW);
    		loadFile("basic65.bin","", false); 
    		runCode();
		}
  		else if (param == F("basicz80")) {
    		cerberus.mode = true;
			cerberus.cls();
    		digitalWrite(CPUSLC, HIGH);
    		loadFile("basicz80.bin","", false); 
    		runCode();	
		}
		else if (param == F("dir")) {
			dir();
		}
		else if (param == F("del")) { 
			cprintStatus(fileIO.deleteFile(getNextParam()));
		}
		else if (param == F("load")) {
			String filename = getNextParam();
			String startAddress = getNextParam();
    		loadFile(filename, startAddress, false);
		}
  		else if (param == F("save")) {
			String startAddress = getNextParam();
			String endAddress = getNextParam();
			String filename = getNextParam();
		    saveFile(filename, startAddress, endAddress);
		}
		else if(param == F("help") || param == F("?")) {
			help();
		}
		else {
			cprintStatus(STATUS_UNKNOWN_COMMAND);
		}
	}
	if (!cerberus.cpuRunning) {
	    storePreviousLine();
	    clearEditLine();
  	}
}

void help() {
	byte y;

	cls();
	cprintString(3, 2,  F("Commands:"));
	y = cprintFileToScreen(3, 4, helpFilename);
	cprintString(3, y++, F("help / ?: Shows this help screen"));
	cprintString(3, y, F("F12 key: Quits CPU program"));
}

void binMove(String startAddr, String endAddr, String destAddr) {
	unsigned int start, finish, destination;							// Memory addresses
	unsigned int i;                                         			// Address counter
	if (startAddr == "") cprintStatus(STATUS_MISSING_OPERAND);			// Missing the filename
	else {
    	start = fromHex(startAddr);          							// Convert hexadecimal address string to unsigned int 
    	if (endAddr == "") cprintStatus(STATUS_MISSING_OPERAND);        // Missing the start 
    	else {
      		finish = fromHex(endAddr);        	 						// Convert hexadecimal address string to unsigned int
      		if (destAddr == "") cprintStatus(STATUS_MISSING_OPERAND);	// Missing the destination
      		else {
        		destination = fromHex(destAddr); 						// Convert hexadecimal address string to unsigned int 
        		if (finish < start) cprintStatus(STATUS_ADDRESS_ERROR);	// Invalid address range 
        		else if ((destination <= finish) && (destination >= start)) cprintStatus(STATUS_ADDRESS_ERROR);	// Destination cannot be within original range
        		else {
          			for (i = start; i <= finish; i++) {
            			cerberus.poke(destination, cerberus.peek(i));
            			destination++;
          			}
          			cprintStatus(STATUS_READY);
        		}
      		}
    	}
  	}
}

// Lists the contents of memory from the given address, in a compact format 
//
void list(String address) {
	byte  i, j;                     // Just counters 
	unsigned int addr;             	// Memory address
	if (address == "") addr = 0;
  	else addr = fromHex(address);	// Convert hexadecimal address string to unsigned int 
  	for (i = 2; i < 25; i++) {
	    cprintString(3, i, "0x");
	    cprintString(5, i, String(addr, HEX));
	    for (j = 0; j < 8; j++) {
      		cprintString(12+(j*3), i, String(cerberus.peek(addr++), HEX)); /** Print bytes in HEX **/
    	}
  	}
}

void runCode() {
	cerberus.cls();
	cerberus.runCode();
}

void stopCode() {
	cerberus.stopCode();
    fileIO.loadFile("chardefs.bin", 0xf000);	// Reset the character definitions in case the CPU changed them 
    cerberus.cls();                     		// Clear screen completely 
    cprintFrames();             				// Reprint the wire frame in case the CPU code messed with it
    cprintBanner();
    cprintStatus(STATUS_DEFAULT);				// Update status bar
    clearEditLine();            				// Clear and display the edit line 
}

// Lists the files in the root directory of uSD card, if available
//
void dir() {
	byte  y = 2;                     	// Screen line 
	byte  x = 0;                     	// Screen column 
	File root;                      	// Root directory of uSD card 
	File entry;                     	// A file on the uSD card 
	cls();
	root = SD.open("/");            	// Go to the root directory of uSD card 
	while (true) {
		entry = root.openNextFile();  	// Open next file
		if (!entry) {                 	// No more files on the uSD card
		  root.close();               	// Close root directory
		  cprintStatus(STATUS_READY);	// Announce completion 
		  break;                      	// Get out of this otherwise infinite while() loop 
		}
		cprintString(3, y, entry.name());
		cprintString(20, y, String(entry.size(), DEC));
		entry.close();                	// Close file as soon as it is no longer needed 
		if (y < 24) y++;              	// Go to the next screen line 
		else {
	    	cprintStatus(STATUS_SCROLL_PROMPT);            		// End of screen has been reached, needs to scroll down 
	    	for (x = 2; x < 40; x++) cprintChar(x, 29, ' ');	// Hide editline while waiting for key press 
	    	while (!keyboard.available());						// Wait for a key to be pressed 
	    	if (keyboard.read() == PS2_ESC) {					// If the user pressed ESC, break and exit 
	      		tone(SOUND, 750, 5);      						// Clicking sound for auditive feedback to key press 
	      		root.close();             						// Close the directory before exiting 
	      		cprintStatus(STATUS_READY);
	      		break;
	    	} else {
	      		tone(SOUND, 750, 5);      						// Clicking sound for auditive feedback to key press 
	      		cls();                    						// Clear the screen and... 
	      		y = 2;                    						// ...go back to the top of the screen 
	    	}
	  	}
	}
}

void saveFile(String filename, String startAddress, String endAddress) {
	unsigned int startAddr;
	unsigned int endAddr;
	int status = STATUS_DEFAULT;
   	if (startAddress == "") {
		status = STATUS_MISSING_OPERAND; 
	}
	else {
		startAddr = fromHex(startAddress);
		if(endAddress == "") {
			status = STATUS_MISSING_OPERAND;
		}
		else {
			endAddr = fromHex(endAddress);
			status = fileIO.saveFile(filename, startAddr, endAddr);
		}
	}
	cprintStatus(status);
}

void loadFile(String filename, String startAddress, bool silent) {
	unsigned int startAddr;
	int status = STATUS_DEFAULT;

	if (startAddress == "") {
		startAddr = ptr_code_start;							// If not otherwise specified, load file into start of code area 
	}
	else {
		startAddr = fromHex(startAddress);					// Convert address string to hexadecimal number 
	}
	status = fileIO.loadFile(filename, startAddr);
	if(!silent) {
		cprintStatus(status);
	}
}

void cprintEditLine () {
  	byte i;
  	for (i = 0; i < 38; i++) cprintChar(i + 2, 29, editLine[i]);
}

// Resets the contents of edit line and reprints it 
//
void clearEditLine() {
  	byte i;
  	editLine[0] = 62;
  	editLine[1] = 0;
  	for (i = 2; i < 38; i++) editLine[i] = 32;
  	pos = 1;
  	cprintEditLine();
}

// Store edit line just executed
//
void storePreviousLine() {
	for (byte i = 0; i < 38; i++) previousEditLine[i] = editLine[i]; 
}

// Print out a status from a code
// REMEMBER: The macro "F()" simply tells the compiler to put the string in code memory, so to save dynamic memory 
//
void cprintStatus(byte status) {
  	switch( status ) {
    	case STATUS_BOOT:
      		center(F("Here we go! Hang on..."));
      		break;
    	case STATUS_READY:
      		center(F("Alright, done!"));
      		break;
    	case STATUS_UNKNOWN_COMMAND:
      		center(F("Darn, unrecognized command"));
      		beep();
      		break;
    	case STATUS_NO_FILE:
      		center(F("Oops, file doesn't seem to exist"));
      		beep();
      		break;
    	case STATUS_CANNOT_OPEN:
      		center(F("Oops, couldn't open the file"));
      		beep();
      		break;
    	case STATUS_MISSING_OPERAND:
      		center(F("Oops, missing an operand!!"));
      		beep();
      		break;
    	case STATUS_SCROLL_PROMPT:
      		center(F("Press a key to scroll, ESC to stop"));
      		break;
    	case STATUS_FILE_EXISTS:
      		center(F("The file already exists!"));
      	break;
    		case STATUS_ADDRESS_ERROR:
      	center(F("Oops, invalid address range!"));
      		break;
    	case STATUS_POWER:
      		center(F("Feel the power of Dutch design!!"));
      		break;
    	default:
			#if config_board == 0
      		cprintString(2, 27, F("      CERBERUS 2080: "));
			#else
      		cprintString(2, 27, F("      CERBERUS 2100: "));
			#endif
      		cprintString(23, 27, cerberus.mode ? F(" Z80, ") : F("6502, "));
      		cprintString(29, 27, cerberus.fast ? F("8 MHz") : F("4 MHz"));
      		cprintString(34, 27, F("     "));
  	}
}

void center(String text) {
  	clearLine(27);
  	cprintString(2+(38-text.length())/2, 27, text);
}

void playJingle() {
	unsigned int i;
	unsigned int f[6] = { 261, 277, 261, 349, 261, 349 };
	unsigned int d[6] = {  50,  50,  50, 500,  50, 900 };

	delay(350);	// Wait for possible preceding keyboard click to end
	for(i = 0; i < 6; i++) {
		delay(150);
		tone(SOUND, f[i], d[i]);
	}
}

// This clears the screen only WITHIN the main frame
//
void cls() {
  	unsigned int y;
  	for (y = 2; y <= 25; y++) {
    	clearLine(y);
	}
}

void clearLine(byte y) {
  	unsigned int x;
  	for (x = 2; x <= 39; x++) {
    	cprintChar(x, y, 32);
	}
}

void cprintFrames() {
  	unsigned int x;
  	unsigned int y;
  	
  	for (x = 2; x <= 39; x++) {	// First print horizontal bars
	    cprintChar(x, 1, 3);
	    cprintChar(x, 30, 131);
	    cprintChar(x, 26, 3);
  	}

  	for (y = 1; y <= 30; y++) {	// Now print vertical bars
	    cprintChar(1, y, 133);
	    cprintChar(40, y, 5);
  	}
}

// Dump a file to screen
//
byte cprintFileToScreen(byte x, byte y, String filename) {
	byte px = x;
	byte py = y;
	byte b;

  	if(SD.exists(filename)) {
    	File f = SD.open(filename);
    	if (f) {
			while(f.available()) {
				b = f.read();
				switch(b) {
					case 0x0A:
						px = x;
						py++;
						break;
					default:
						cprintChar(px++, py, b);
						break;
				}
			}
      		f.close();
    	}
  	}	
	return py;
}

// Load the CERBERUS icon image on the screen
//
void cprintBanner() {
	cprintFileToScreen(2, 2, bannerFilename);
}

void cprintString(byte x, byte y, String text) {
  	unsigned int i;
  	for (i = 0; i < text.length(); i++) {
	    if (((x + i) > 1) && ((x + i) < 40)) {
			cprintChar(x + i, y, text[i]);
		}
  	}
}

void cprintChar(byte x, byte y, byte token) {
  	unsigned int address = 0xF800 + ((y - 1) * 40) + (x - 1); // Video memory addresses start at 0XF800
  	cerberus.poke(address, token);
}
