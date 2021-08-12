
# releases

These releases are compiled on an ad-hoc basis and have been tested on a Cerberus board before release.

Note that this version currently requires the custom CAT code to be installed on the ATmega328p before it will run correctly. This can be found in the [cat](cat) folder, along with instructions on building and installing it. I would suggest updating it as a matter of course if you start using a newer version of BBC Basic

You will need to rename the file to an 8.3 filename with no special characters to make it easier to load from the Cerberus BIOS screen, i.e. `bbcbasic.bin`

##### 20210812: Version 0.02
- Bug fixes to editor, the cursor keys now work when editing the current line
- Added a cursor
- Added LOAD and SAVE commands to load and save BASIC files to SD card
- Added the following * commands
	- `*BYE`: Exit to BIOS (doesn't work properly yet)
	- `*DELETE filename`: Delete a file from the SD card
	- `*ERASE filename`: Alias for DELETE
	- `*FX`: Execute an FX command
	- `*MEMDUMP start length`: Output a hexdump of selected memory range to screen

##### 20210810: Version 0.01
- Key scanning and output routines
- Screen output routines
- Plot and Draw graphics primitives added
- Extended GET and GET$ to read characters off screen   

Dean Belfield
www.breakintoprogram.co.uk
