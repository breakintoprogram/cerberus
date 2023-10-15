#!/usr/bin/python3

# Title:		Send a file to the Cerberus
# Author:		Dean Belfield
# Created:		15/10/2023
# Last Updated:	15/10/2023
#
# Modinfo:

import sys
import os
import serial
import time

# Configuration
#
startAddress = 0x0205
blockSize = 10

# Configure serial port here. Last parameter is timeout to stop reading data, in seconds: 
#
s = serial.Serial('/dev/ttyUSB1', 115200, serial.EIGHTBITS, serial.PARITY_NONE, serial.STOPBITS_ONE)

# Open the file for reading
#
name = sys.argv[1]										# Get the filename
fullPath = os.path.expanduser(name)						# Expand the full path to the file and
file = open(fullPath, "rb")								# Open it up as a binary file

time.sleep(7)											# Wait for the Cerberus to reset			

count = 0												# Number of bytes written
chksA = 1												# Checksums
chksB = 0
reset = True

# Iterate through the file
#
while True:
	data = file.read(1)									# Read 1 byte into the buffer data
	if not data:
		reset = True

	if reset:
		if count > 0:
			s.write(bytes([0x0D]))						# Enter that line
			startAddress += count						# Update the start address for the next line
			checksum = F"{chksA << 8 | chksB:X}"		# Calculate the checksum on this side
			response = s.readline().decode().rstrip()	# Fetch the response
			count = 0									# Reset for next line
			print(F" > {response} : {checksum}", end="")
			if not response.endswith(checksum):			# If the checksums don't match then error
				print(" - Error: Mismatched Checksum")
				break
			elif not data:								# If there is no more data then
				print(" - End")
				break									# end
			else:
				print(" - OK")
		chksA = 1										# Reset the checksums
		chksB = 0
		reset = False
		s.write(F"0x{startAddress:04X}".encode())		# Write out the start address
		print(F"0x{startAddress:04X}", end="")
		
	byte = data[0]										# Fetch the byte 
	print(F" {byte:02X}", end="")
	chksA = (chksA + byte) % 256						# Calculate the checksums
	chksB = (chksB + chksA) % 256
	s.write(F" {byte:02x}".encode())					# Write the byte out
	count += 1
	if(count >= blockSize):
		reset = True 

file.close()											# We've done so close the files