#ifndef HARDWARE_H
#define HARDWARE_H

extern void cpuInterrupt(void);

class Hardware {
	public:
		Hardware(int boardRevision) :
			boardRevision(boardRevision)
		{	
			cpuRunning = false;
			interruptFlag = false;
			expFlag = false;
		};

		void			initialise();
		void			resetCPU();
		void			stopCode();
		void			runCode();
		void			xbusHandler();
		void			testMemory();

		void			setMode(bool mode);
		void			setFast(bool fast);

		void			cls();

		void			poke(unsigned int address, byte data);
		void			poke(unsigned int address, unsigned int data);
		void			poke(unsigned int address, unsigned long data);
		boolean			poke(unsigned int address, String text);
		byte			peek(unsigned int address);
		unsigned int	peekWord(unsigned int address);
		boolean			peekString(unsigned int address, byte * dest, int max);

		void(* reset)(void) = 0;

		volatile bool	mode;
		volatile bool	fast;
		volatile bool	cpuRunning;
		volatile bool	interruptFlag;
		volatile bool	expFlag;

	private:
		void			resetCPU(bool cpuslc);
		unsigned int	addressTranslate (unsigned int virtualAddress);
		uint8_t 		readSetting(int idx, uint8_t bound_low, uint8_t bound_high, uint8_t default_value);
		byte			readShiftRegister();
		void			setShiftRegister(unsigned int address, byte data);

		int				boardRevision;
};

void Hardware::initialise() {
	// Read the default settings from EEPROM
	//
	mode = this->readSetting(config_eeprom_address_mode, 0, 1, config_default_mode);
	fast = this->readSetting(config_eeprom_address_fast, 0, 1, config_default_fast);

	// Declare the pins
	//
  	pinMode(SO, OUTPUT);
  	pinMode(SI, INPUT);               // There will be pull-up and pull-down resistors in circuit
  	pinMode(SC, OUTPUT);
  	pinMode(AOE, OUTPUT);
  	pinMode(LD, OUTPUT);
  	pinMode(RW, OUTPUT);
  	pinMode(CPUSPD, OUTPUT);
  	pinMode(KCLK, INPUT_PULLUP);      // But here we need CAT's internal pull-up resistor
  	pinMode(KDAT, INPUT_PULLUP);      // And here too
  	pinMode(CPUSLC, OUTPUT);
  	pinMode(CPUIRQ, OUTPUT);
  	pinMode(CPUGO, OUTPUT);
  	pinMode(CPURST, OUTPUT);
  	pinMode(SOUND, OUTPUT);

	// Write default values to some of the output pins
	//
  	digitalWrite(RW, HIGH);
  	digitalWrite(SO, LOW);
  	digitalWrite(AOE, LOW);
  	digitalWrite(LD, LOW);
  	digitalWrite(SC, LOW);
  	digitalWrite(CPUSPD, fast);
  	digitalWrite(CPUSLC, mode);
  	digitalWrite(CPUIRQ, LOW);
  	digitalWrite(CPUGO, LOW);
  	digitalWrite(CPURST, LOW);
	
	// Attach an interrupt to XIRQ so to react to the expansion card timely (Cerberus 2100 only)
	//
	if(boardRevision == 1) {
		pinMode(XBUSACK, OUTPUT);
    	digitalWrite(XBUSACK, HIGH);
  		cli();	
		PCICR  |= 0b00001000;				// Enables Port E Pin Change Interrupts
		PCMSK3 |= 0b00001000;				// Enable Pin Change Interrupt on XIRQ
		sei();	
	}
}

// Reset both CPUs
//
void Hardware::resetCPU() {            	
  	digitalWrite(CPURST, LOW);
	resetCPU(LOW);							// Reset the 6502
	resetCPU(HIGH);							// Reset the Z80
  	if(!mode) {								// Select the correct chip for the mode
		digitalWrite(CPUSLC, LOW);
	}
}

void Hardware::resetCPU(bool cpuslc) {
  	digitalWrite(CPUSLC, cpuslc);
  	digitalWrite(CPUGO, HIGH);
  	delay(50);
  	digitalWrite(CPURST, HIGH);
  	digitalWrite(CPUGO, LOW);
  	delay(50);
  	digitalWrite(CPURST, LOW);
}

// The XBUS handler (Cerberus 2100 only)
//
void Hardware::xbusHandler() {
	if(boardRevision < 1) {
		return;
	}
	// Now we deal with bus access requests from the expansion card
	//
	if(digitalRead(XBUSREQ) == LOW) { 					// The expansion card is requesting bus access... 
    	if(cpuRunning) { 								// If a CPU is running (the internal buses being therefore not tristated)...
      		digitalWrite(CPUGO, LOW);					// ...first pause the CPU and tristate the buses...
      		digitalWrite(XBUSACK, LOW); 				// ...then acknowledge request; buses are now under the control of the expansion card
      		while (digitalRead(XBUSREQ) == LOW); 		// Wait until the expansion card is done...
      		digitalWrite(XBUSACK, HIGH); 				// ...then let the expansion card know that the buses are no longer available to it
      		digitalWrite(CPUGO, HIGH); 					// Finally, let the CPU run again
    	} else { 										// If a CPU is NOT running...
      		digitalWrite(XBUSACK, LOW); 				// Acknowledge request; buses are now under the control of the expansion card
      		while (digitalRead(XBUSREQ) == LOW);	 	// Wait until the expansion card is done...
      		digitalWrite(XBUSACK, HIGH); 				// ...then let the expansion card know that the buses are no longer available to it
    	}
  	}
  	// And finally, deal with the expansion flag (which will be 'true' if there has been an XIRQ strobe from an expansion card)
	//
  	if(expFlag) {
    	if(cpuRunning) {
      		digitalWrite(CPUGO, LOW); 					// Pause the CPU and tristate its buses **/
      		poke(ptr_xbus_flag, byte(0x01)); 			// Flag that there is data from the expansion card waiting for the CPU in memory
      		digitalWrite(CPUGO, HIGH);					// Let the CPU go
      		digitalWrite(CPUIRQ, HIGH); 				// Trigger an interrupt so the CPU knows there's data waiting for it in memory
      		digitalWrite(CPUIRQ, LOW);
    	}
    	expFlag = false; 								// Reset the flag
  	}
}

void Hardware::stopCode() {
    cpuRunning = false;         						// Reset this flag 
    Timer1.detachInterrupt();							// Stop the interrupt timer
    digitalWrite(CPURST, HIGH); 						// Reset the CPU to bring its output signals back to original states
    digitalWrite(CPUGO, LOW);   						// Tristate its buses to high-Z
    delay(50);                  						// Give it some time
    digitalWrite(CPURST, LOW);  						// Finish reset cycle
}

void Hardware::runCode() {
	byte runL = ptr_code_start & 0xFF;
	byte runH = ptr_code_start >> 8;

	// NB:
	// - Byte at ptr_outbox_flag is the new mail flag
	// - Byte at ptr_outbox_data is the mail box
	//
	poke(ptr_outbox_flag, byte(0x00));					// Reset outbox mail flag
	poke(ptr_outbox_data, byte(0x00));					// Reset outbox mail data
	poke(ptr_inbox_flag, byte(0x00));					// Reset inbox mail flag

	if(!mode) {											// 6502 mode
		poke(0xFFFA, byte(0xB0));
		poke(0xFFFB, byte(0xFC));
		poke(0xFCB0, byte(0x40));						// The interrupt service routine simply returns FCB0: RTI (40)
		poke(0xFFFC, runL);								// Set reset vector to the beginning of the code area 
		poke(0xFFFD, runH);
	}
	else {                								// Z80 mode
		poke(0x0066, byte(0xED));						// The NMI service routine of the Z80 simply returns 0066: RETN (ED 45)
		poke(0x0067, byte(0x45));
		#if ptr_code_start != 0x0000	  
	  	poke(0x0000, byte(0xC3));						// The Z80 fetches the first instruction from 0x0000, so put a jump to the code area there (C3 ll hh)
	  	poke(0x0001, runL);
	  	poke(0x0002, runH);
	  	#endif
	}
	cpuRunning = true;
	digitalWrite(CPURST, HIGH); 						// Reset the CPU
	digitalWrite(CPUGO, HIGH);  						// Enable CPU buses and clock
	delay(50);
	digitalWrite(CPURST, LOW);  						// CPU should now initialize and then go to its reset vector

	#if config_enable_nmi == 1
  	Timer1.initialize(20000);
  	Timer1.attachInterrupt(cpuInterrupt); 				// Interrupt every 0.02 seconds - 50Hz
  	#endif
}

// Read a setting from the EEPROM
//
uint8_t Hardware::readSetting(int idx, uint8_t bound_low, uint8_t bound_high, uint8_t default_value) {
	uint8_t b = EEPROM.read(idx);
	if(b < bound_low || b > bound_high) {
		b = default_value;
		EEPROM.write(idx, b);
	}
	return b;
}

unsigned int Hardware::addressTranslate (unsigned int virtualAddress) {
  	byte numberVirtualRows;
  	numberVirtualRows = (virtualAddress - 0xF800) / 38;
  	return((virtualAddress + 43) + (2 * (numberVirtualRows - 1)));
}

byte Hardware::readShiftRegister() {
  	return shiftIn(SI, SC, MSBFIRST);
}

void Hardware::setShiftRegister(unsigned int address, byte data) { 
  	shiftOut(SO, SC, LSBFIRST, address);      	// First 8 bits of address 
  	shiftOut(SO, SC, LSBFIRST, address >> 8);	// Then the remaining 8 bits 
  	shiftOut(SO, SC, LSBFIRST, data);         	// Finally, a byte of data 
}

void Hardware::poke(unsigned int address, byte data) {
  	setShiftRegister(address, data);
  	digitalWrite(AOE, HIGH);					// Enable address onto bus 
  	digitalWrite(RW, LOW);						// Begin writing 
  	digitalWrite(RW, HIGH);						// Finish up
  	digitalWrite(AOE, LOW);
}

void Hardware::poke(unsigned int address, unsigned int data) {
	poke(address, byte(data & 0xFF));
	poke(address + 1, byte((data >> 8) & 0xFF));
}

void Hardware::poke(unsigned int address, unsigned long data) {
	poke(address, byte(data & 0xFF));
	poke(address + 1, byte((data >> 8) & 0xFF));
	poke(address + 2, byte((data >> 16) & 0xFF));
	poke(address + 3, byte((data >> 24) & 0xFF));
}

boolean Hardware::poke(unsigned int address, String text) {
	unsigned int i;
	for(i = 0; i < text.length(); i++) {
		poke(address + i, byte(text[i]));
	}
	poke(address + i, byte(0));
	return true;
}

byte Hardware::peek(unsigned int address) {
  	byte data = 0;
  	setShiftRegister(address, data);
  	digitalWrite(AOE, HIGH);      // Enable address onto us
	//
  	// This time we do NOT enable the data outputs of the shift register, as we are reading 
	//
  	digitalWrite(LD, HIGH);       // Prepare to latch byte from data bus into shift register 
  	digitalWrite(SC, HIGH);       // Now the clock tics, so the byte is actually latched 
  	digitalWrite(LD, LOW);
  	digitalWrite(AOE, LOW);
  	data = readShiftRegister();
  	return data;
}

unsigned int Hardware::peekWord(unsigned int address) {
	return peek(address) + (256 * peek(address+1));
}

boolean Hardware::peekString(unsigned int address, byte * dest, int max) {
	unsigned int i;
	byte c;
	for(i = 0; i < max; i++) {
		c = peek(address + i);
		dest[i] = c;
		if(c == 0) return true;
	}
	return false;
}

// Tests that all four memories are accessible for reading and writing 
//
void Hardware::testMemory() {
  	unsigned int x;
  	byte i = 0;
    for (x = 0; x < 874; x++) {
    	poke(x, i);												// Write to low memory **/
    	poke(0x8000 + x, peek(x));								// Read from low memory and write to high memory 
    	poke(addressTranslate(0xF800 + x), peek(0x8000 + x));	// Read from high mem, write to VMEM, read from character mem 
    	if (i < 255) i++;
    	else i = 0;
  	}
}

void Hardware::setMode(bool value) {
    mode = value;
    EEPROM.write(config_eeprom_address_mode, mode);
    digitalWrite(CPUSLC, mode);
}

void Hardware::setFast(bool value) {
    fast = value;
    EEPROM.write(config_eeprom_address_fast, fast);
    digitalWrite(CPUSPD, fast);
}

// This clears the entire screen 
//
void Hardware::cls() {
  	unsigned int x;
  	for (x = 0; x < 1200; x++) {
	    poke(0xF800 + x, byte(32));		// Video memory addresses start at 0XF800
	}
}

#endif // HARDWARE_H