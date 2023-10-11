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

// And all the internal includes
//
#include "config.h"
#include "hardware.h"

// Now some stuff required by the libraries in use
//
const int	chipSelect = CS;
const int	DataPin = KDAT;
const int	IRQpin = KCLK;

const char * bannerFilename = "cerbicon.img";
const char * helpFilename = "help.txt";

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

volatile char editLine[38];					// Current edit line buffer
volatile char previousEditLine[38];			// Previous edit line buffer (allows for repeating previous command)

volatile byte pos = 1;						// Position in edit line currently occupied by cursor 

File		cd;								// Used by BASIC directory commands
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
      		tone(SOUND, 50, 150);
      		delay(500);
    	}
  	}
  	// Load character defs into memory
	//
  	if(load("chardefs.bin", 0xf000) != STATUS_READY) {
		tone(SOUND, 50, 150);
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
		    		enter();
	    			break;
	    		case PS2_UPARROW:
		     	   	for (i = 0; i < 38; i++) editLine[i] = previousEditLine[i];
	        		i = 0;
        			while (editLine[i] != 0) i++;
        			pos = i;
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
	return delFile((char *)editLine);	
}

// Handle LOAD command from BASIC
//
int cmdLoad(unsigned int address) {
	unsigned int startAddr = cerberus.peekWord(address);
	unsigned int length = cerberus.peekWord(address + 2);
	cerberus.peekString(address + 4, (byte *)editLine, 38);
	return load((char *)editLine, startAddr);
}

// Handle SAVE command from BASIC
//
int cmdSave(unsigned int address) {
	unsigned int startAddr = cerberus.peekWord(address);
	unsigned int length = cerberus.peekWord(address + 2);
	cerberus.peekString(address + 4, (byte *)editLine, 38);
	return save((char *)editLine, startAddr, startAddr + length - 1);
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

/************************************************************************************************/
void enter() {  /** Called when the user presses ENTER, unless a CPU program is being executed **/
/************************************************************************************************/
  unsigned int addr;                /** Memory addresses **/
  byte data;                        /** A byte to be stored in memory **/
  byte i;                           /** Just a counter **/
  String nextWord, nextNextWord, nextNextNextWord; /** General-purpose strings **/
  nextWord = getNextWord(true);     /** Get the first word in the edit line **/
  nextWord.toLowerCase();           /** Ignore capitals **/
  if( nextWord.length() == 0 ) {    /** Ignore empty line **/
    Serial.println(F("OK"));
    return;
  }
  /** MANUAL ENTRY OF OPCODES AND DATA INTO MEMORY *******************************************/
  if ((nextWord.charAt(0) == '0') && (nextWord.charAt(1) == 'x')) { /** The user is entering data into memory **/
    nextWord.remove(0,2);                       /** Removes the "0x" at the beginning of the string to keep only a HEX number **/
    addr = strtol(nextWord.c_str(), NULL, 16);  /** Converts to HEX number type **/
    nextNextWord = getNextWord(false);          /** Get next byte **/
    byte chkA = 1;
    byte chkB = 0;
    while (nextNextWord != "") {                /** For as long as user has typed in a byte, store it **/
      if(nextNextWord.charAt(0) != '#') {
        data = strtol(nextNextWord.c_str(), NULL, 16);/** Converts to HEX number type **/
        while( cerberus.peek(addr) != data ) {          /** Serial comms may cause writes to be missed?? **/
          cerberus.poke(addr, data);
        }
        chkA += data;
        chkB += chkA;
        addr++;
      }
      else {
        nextNextWord.remove(0,1);
        addr = strtol(nextNextWord.c_str(), NULL, 16);
        if( addr != ((chkA << 8) | chkB) ) {
          cprintString(28, 26, nextWord);
          tone(SOUND, 50, 50);
        }
      }
      nextNextWord = getNextWord(false);  /** Get next byte **/
    }
    #if config_dev_mode == 0
    cprintStatus(STATUS_READY);
    cprintString(28, 27, nextWord);
    #endif
    Serial.print(nextWord);
    Serial.print(' ');
    Serial.println((unsigned int)((chkA << 8) | chkB), HEX);
    
  /** LIST ***********************************************************************************/
  } else if (nextWord == F("list")) {     /** Lists contents of memory in compact format **/
    cls();
    nextWord = getNextWord(false);        /** Get address **/
    list(nextWord);
    cprintStatus(STATUS_READY);
  /** CLS ************************************************************************************/
  } else if (nextWord == F("cls")) {      /** Clear the main window **/
    cls();
    cprintStatus(STATUS_READY);
  /** TESTMEM ********************************************************************************/
  } else if (nextWord == F("testmem")) {  /** Checks whether all four memories can be written to and read from **/
    cls();
    cerberus.testMemory();
    cprintStatus(STATUS_READY);
  /** 6502 ***********************************************************************************/
  } else if (nextWord == F("6502")) {     /** Switches to 6502 mode **/
	cerberus.setMode(0);
    cprintStatus(STATUS_READY);
  /** Z80 ***********************************************************************************/
  } else if (nextWord == F("z80")) {      /** Switches to Z80 mode **/
	cerberus.setMode(1);
    cprintStatus(STATUS_READY);
  /** RESET *********************************************************************************/
  } else if (nextWord == F("reset")) {
    cerberus.reset();				      /** This resets CAT and, therefore, the CPUs too **/
  /** FAST **********************************************************************************/
  } else if (nextWord == F("fast")) {     /** Sets CPU clock at 8 MHz **/
	cerberus.setFast(1);
	cprintStatus(STATUS_READY);
  /** SLOW **********************************************************************************/
  } else if (nextWord == F("slow")) {     /** Sets CPU clock at 4 MHz **/
	cerberus.setFast(0);
    cprintStatus(STATUS_READY);
  /** DIR ***********************************************************************************/
  } else if (nextWord == F("dir")) {      /** Lists files on uSD card **/
    dir();
  /** DEL ***********************************************************************************/
  } else if (nextWord == F("del")) {      /** Deletes a file on uSD card **/
    nextWord = getNextWord(false);
    catDelFile(nextWord);
  /** LOAD **********************************************************************************/
  } else if (nextWord == F("load")) {     /** Loads a binary file into memory, at specified location **/
    nextWord = getNextWord(false);        /** Get the file name from the edit line **/
    nextNextWord = getNextWord(false);    /** Get memory address **/
    catLoad(nextWord, nextNextWord, false);
  /** RUN ***********************************************************************************/
  } else if (nextWord == F("run")) {      /** Runs the code in memory **/
    for (i = 0; i < 38; i++) previousEditLine[i] = editLine[i]; /** Store edit line just executed **/
    runCode();
  /** SHORTCUTS FOR BASIC *******************************************************************/
  } else if (nextWord == F("basic6502")) {
    cerberus.mode = false;
	cerberus.cls();
    digitalWrite(CPUSLC, LOW);
    catLoad("basic65.bin","", false); 
    runCode();
  } else if (nextWord == F("basicz80")) {
    cerberus.mode = true;
	cerberus.cls();
    digitalWrite(CPUSLC, HIGH);
    catLoad("basicz80.bin","", false); 
    runCode();	
  /** SAVE **********************************************************************************/
  } else if (nextWord == F("save")) {
    nextWord = getNextWord(false);						/** Start start address **/
    nextNextWord = getNextWord(false);					/** End address **/
    nextNextNextWord = getNextWord(false);				/** Filename **/
    catSave(nextNextNextWord, nextWord, nextNextWord);
  /** MOVE **********************************************************************************/
  } else if (nextWord == F("move")) {
    nextWord = getNextWord(false);
    nextNextWord = getNextWord(false);
    nextNextNextWord = getNextWord(false);
    binMove(nextWord, nextNextWord, nextNextNextWord);
  /** HELP **********************************************************************************/
  } else if ((nextWord == F("help")) || (nextWord == F("?"))) {
    help();
    cprintStatus(STATUS_POWER);
  /** ALL OTHER CASES ***********************************************************************/
  } else cprintStatus(STATUS_UNKNOWN_COMMAND);
  if (!cerberus.cpuRunning) {
    storePreviousLine();
    clearEditLine();                   /** Reset edit line **/
  }
}

String getNextWord(bool fromTheBeginning) {
  /** A very simple parser that returns the next word in the edit line **/
  static byte  initialPosition;    /** Start parsing from this point in the edit line **/
  byte  i, j, k;                   /** General-purpose indices **/
  if (fromTheBeginning) initialPosition = 1; /** Starting from the beginning of the edit line **/
  i = initialPosition;            /** Otherwise, continuing on from where we left off in previous call **/
  while ((editLine[i] == 32) || (editLine[i] == 44)) i++; /** Ignore leading spaces or commas **/
  j = i + 1;                      /** Now start indexing the next word proper **/
  /** Find the end of the word, marked either by a space, a comma or the cursor **/
  while ((editLine[j] != 32) && (editLine[j] != 44) && (editLine[j] != 0)) j++;
  char nextWord[j - i + 1];       /** Create a buffer (the +1 is to make space for null-termination) **/
  for (k = i; k < j; k++) nextWord[k - i] = editLine[k]; /** Transfer the word to the buffer **/
  nextWord[j - i] = 0;            /** Null-termination **/
  initialPosition = j;            /** Next time round, start from here, unless... **/
  return (nextWord);              /** Return the contents of the buffer **/
}

void help() {
	byte x = 3;
	byte y = 4;
	byte b;
	
	cls();
	cprintString(3, 2,  F("Commands:"));

  	if(SD.exists(helpFilename)) {
    	File f = SD.open(helpFilename);		// Open the help file
    	if (f) {
			while(f.available()) {
				b = f.read();
				if(b >= 32) {
					cprintChar(x++, y, b);
				}
				if(b == 0x0A) {
					x = 3;
					y++;
				}
			}
      		f.close();
    	}
  	}
	cprintString(3, y++, F("help / ?: Shows this help screen"));
	cprintString(3, y, F("ESC key: Quits CPU program"));
}

void binMove(String startAddr, String endAddr, String destAddr) {
  unsigned int start, finish, destination;                /** Memory addresses **/
  unsigned int i;                                         /** Address counter **/
  if (startAddr == "") cprintStatus(STATUS_MISSING_OPERAND);                   /** Missing the file's name **/
  else {
    start = strtol(startAddr.c_str(), NULL, 16);          /** Convert hexadecimal address string to unsigned int **/
    if (endAddr == "") cprintStatus(STATUS_MISSING_OPERAND);                   /** Missing the file's name **/
    else {
      finish = strtol(endAddr.c_str(), NULL, 16);         /** Convert hexadecimal address string to unsigned int **/
      if (destAddr == "") cprintStatus(STATUS_MISSING_OPERAND);                /** Missing the file's name **/
      else {
        destination = strtol(destAddr.c_str(), NULL, 16); /** Convert hexadecimal address string to unsigned int **/
        if (finish < start) cprintStatus(STATUS_ADDRESS_ERROR);              /** Invalid address range **/
        else if ((destination <= finish) && (destination >= start)) cprintStatus(STATUS_ADDRESS_ERROR); /** Destination cannot be within original range **/  
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
  byte  i, j;                      // Just counters 
  unsigned int addr;             	 // Memory address
  if (address == "") addr = 0;
  else addr = strtol(address.c_str(), NULL, 16); /** Convert hexadecimal address string to unsigned int **/
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
    load("chardefs.bin", 0xf000);		// Reset the character definitions in case the CPU changed them 
    cerberus.cls();                     // Clear screen completely 
    cprintFrames();             		// Reprint the wire frame in case the CPU code messed with it
    cprintBanner();
    cprintStatus(STATUS_DEFAULT);		// Update status bar
    clearEditLine();            		// Clear and display the edit line 
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

void catDelFile(String filename) {
	cprintStatus(delFile(filename));
}

// Deletes a file from the uSD card 
//
int delFile(String filename) {
	int status = STATUS_DEFAULT;
  	if (!SD.exists(filename)) {
		status = STATUS_NO_FILE;								// The file doesn't exist, so stop with error 
	}
  	else {
	    SD.remove(filename);          							// Delete the file
	    status = STATUS_READY;
  	}
	return status;
}

void catSave(String filename, String startAddress, String endAddress) {
	unsigned int startAddr;
	unsigned int endAddr;
	int status = STATUS_DEFAULT;
   	if (startAddress == "") {
		status = STATUS_MISSING_OPERAND; 
	}
	else {
		startAddr = strtol(startAddress.c_str(), NULL, 16);
		if(endAddress == "") {
			status = STATUS_MISSING_OPERAND;
		}
		else {
			endAddr = strtol(endAddress.c_str(), NULL, 16);
			status = save(filename, startAddr, endAddr);
		}
	}
	cprintStatus(status);
}

// Saves contents of a region of memory to a file on uSD card
//
int save(String filename, unsigned int startAddress, unsigned int endAddress) {
	int status = STATUS_DEFAULT;
  	unsigned int i;                                     		// Memory address counter
  	byte data;                                          	// Data from memory
  	File dataFile;                                      	// File to be created and written to
	if (endAddress < startAddress) {	
		status = STATUS_ADDRESS_ERROR;            			// Invalid address range
	}	
	else {	
		if (filename == "") {	
			status = STATUS_MISSING_OPERAND;          		// Missing the file's name 
		}	
		else {	
			if (SD.exists(filename)) {	
				status = STATUS_FILE_EXISTS;   				// The file already exists, so stop with error 
			}
			else {
				dataFile = SD.open(filename, FILE_WRITE);	// Try to create the file
				if (!dataFile) {
					status = STATUS_CANNOT_OPEN;           	// Cannot create the file 
				}
				else {                                    	// Now we can finally write into the created file 
					for(i = startAddress; i <= endAddress; i++) {
						data = cerberus.peek(i);
						dataFile.write(data);
					}
					dataFile.close();
					status = STATUS_READY;
				}
			}
		}
	}
	return status;
}

void catLoad(String filename, String startAddress, bool silent) {
	unsigned int startAddr;
	int status = STATUS_DEFAULT;
	if (startAddress == "") {
		startAddr = ptr_code_start;							// If not otherwise specified, load file into start of code area 
	}
	else {
		startAddr = strtol(startAddress.c_str(), NULL, 16);	// Convert address string to hexadecimal number 
	}
	status = load(filename, startAddr);
	if(!silent) {
		cprintStatus(status);
	}
}

// Loads a binary file from the uSD card into memory
//
int load(String filename, unsigned int startAddr) {
  File dataFile;                                			// File for reading from on SD Card, if present
  unsigned int addr = startAddr;         		       			// Address where to load the file into memory
  int status = STATUS_DEFAULT;
  if (filename == "") {
	  status = STATUS_MISSING_OPERAND;
  }
  else {
    if (!SD.exists(filename)) {
		status = STATUS_NO_FILE;							// The file does not exist, so stop with error 
	} 
    else {
      	dataFile = SD.open(filename);           			// Open the binary file 
      	if (!dataFile) {
			status = STATUS_CANNOT_OPEN; 					// Cannot open the file 
	  	}
      	else {
        	while (dataFile.available()) {					// While there is data to be read..
          	cerberus.poke(addr++, byte(dataFile.read()));	// Read data from file and store it in memory 
          	if (addr == 0) {                    			// Break if address wraps around to the start of memory 
            	dataFile.close();
            	break;
          	}
        }
        dataFile.close();
		status = STATUS_READY;
      }
    }
  }
  return status;
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
      		tone(SOUND, 50, 150);
      		break;
    	case STATUS_NO_FILE:
      		center(F("Oops, file doesn't seem to exist"));
      		tone(SOUND, 50, 150);
      		break;
    	case STATUS_CANNOT_OPEN:
      		center(F("Oops, couldn't open the file"));
      		tone(SOUND, 50, 150);
      		break;
    	case STATUS_MISSING_OPERAND:
      		center(F("Oops, missing an operand!!"));
      		tone(SOUND, 50, 150);
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

// Load the CERBERUS icon image on the screen
//
void cprintBanner() {
	String	tokenText;
  	byte 	inChar;

  	if(!SD.exists(bannerFilename)) {
		tone(SOUND, 50, 150); 						// Tone out an error if file is not available
	}
  	else {		
    	File dataFile2 = SD.open(bannerFilename);	// Open the image file
    	if (!dataFile2) {
			tone(SOUND, 50, 150);     				// Tone out an error if file can't be opened 
		}
    	else {
      		for (byte y = 2; y <= 25; y++) {
        		for (byte x = 2; x <= 39; x++) {
	          		tokenText = "";
          			while (isDigit(inChar = dataFile2.read())) {
						tokenText += char(inChar);
					}
          			cprintChar(x, y, tokenText.toInt());
        		}
			}
      		dataFile2.close();
    	}
  	}
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
