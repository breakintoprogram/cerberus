#ifndef FILEIO_H
#define FILEIO_H

extern	Hardware	cerberus;

class FileIO {
	public:
		FileIO() {};

		int		loadFile(String filename, unsigned int startAddr);
		int		saveFile(String filename, unsigned int startAddress, unsigned int endAddress);
		int		deleteFile(String filename);
};

// Loads a binary file from the uSD card into memory
//
int FileIO::loadFile(String filename, unsigned int startAddr) {
	File dataFile;                                				// File for reading from on SD Card, if present
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
      	dataFile = SD.open(filename);           				// Open the binary file 
      	if (!dataFile) {
			status = STATUS_CANNOT_OPEN; 						// Cannot open the file 
	  	}
      	else {
        	while (dataFile.available()) {						// While there is data to be read..
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

// Saves contents of a region of memory to a file on uSD card
//
int FileIO::saveFile(String filename, unsigned int startAddress, unsigned int endAddress) {
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

// Deletes a file from the uSD card 
//
int FileIO::deleteFile(String filename) {
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

#endif // FILEIO_H
