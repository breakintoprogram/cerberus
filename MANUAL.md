# manual

In addition to the core BBC Basic for Z80 core language (details of which [can be found here](bbcbasic.txt)), BBC Basic for Cerberus 2080 adds the following functionality:

## Editor

A line editor is provided, that allows the user to enter a line of code up to 255 characters long. A cursor can be moved around this line, and text can be deleted or inserted at the current character position.

## BASIC

The following statements differ from the BBC Basic standard:

### GET(x,y)

Read a character code from the screen position X, Y. If the character cannot be read, return -1

Example:

- `A = GET(0,0)` Read character code from screen position(0,0)

### GET$(x,y)

As GET(x,y), but return a string rather than a character code

Example:

- `A$ = GET$(0,0)` Read character code from screen position(0,0)

### PLOT type,x,y

The only plot modes supported currently are:

- `PLOT 0, X, Y` Draw a line from last plot position to X, Y
- `PLOT 64, X, Y` Plot a point

### GCOL mode, colour

Sets the graphic colour, as per COLOUR

Mode values are:
- 0 and 1 set the pixel
- 2 and 6 clear the pixel
- 3 and 4 invert the pixel

This is an attempt to stick to the BBC BASIC standard with 1-bit graphics

The colour part is currently ignored, but will at some point be:

- 0: Black
- 1: White

### VDU

The VDU command is a work-in-progress with a handful of mappings implemented:

- `VDU 8` Backspace
- `VDU 9` Advance one character
- `VDU 10` Line feed
- `VDU 11` Move cursor up one line
- `VDU 12` CLS
- `VDU 13` Carriage return
- `VDU 16` CLG
- `VDU 17,col` COLOUR col
- `VDU 18,mode,col` GCOL mode,col
- `VDU 19,l,r,g,b` COLOUR l,r,g,b
- `VDU 22,n` Mode n
- `VDU 23,c,b1,b2,b3,b4,b5,b6,b7,b8` Define UDG c
- `VDU 25,mode,x;y;` PLOT mode,x,y
- `VDU 29,x;y;` Set graphics origin to x,y
- `VDU 30` Home cursor
- `VDU 31,x,y` TAB(x,y)

Note that some of the commands are not supported - see the bottom of this page for more details

Examples:

`VDU 25,64,128;88;` Plot point in middle of screen

`VDU 22,1` Change to Mode 1

### SOUND

The same format as the BBC Model B sound command, though is not asynchronous. Volume and channel parameters are ignored.

Example:

`SOUND 0,0,100,50`

## STAR commands

The star commands are all prefixed with an asterisk. These commands do not accept variables or expressions as parameters. Parameters are separated by spaces. Numeric parameters can be specified in hexadecimal by prefixing with an '&' character. Paths are unquoted. 

If you need to pass a parameter to a star command, call it using the OSCLI command, for example:

- `LET T% = 3: OSCLI("TURBO " + STR$(T%))`

### BYE

Exits BBC Basic by doing a soft reset (does not work on emulators)

### ERASE

Erase a file

- `*ERASE test.bbc` Erase the file test.bbc

Aliases: DELETE

### MEMDUMP start len

List contents of memory, hexdump and ASCII.

- `*MEMDUMP 0 200` Dump the first 200 bytes of ROM

### FX n

Implemented as OSBYTE

- `*FX 19`: Wait for an NMI interrupt

### LOAD filename start length

Load a file into memory

### SAVE filename start length

Save a block of memory as a file

## Other considerations

### Graphics

A psuedo-graphics system has been implemented using the Cerberus 2x2 block-graphics characters, in a similar fashion to the ZX81. This provides an 80x60 graphics resolution.

### Unsupported Commands

The following commands are not supported:

- `MODE`: Unlikely to be implemented as there is only one mode
- `CLG`: There is no graphics bitmap
- `COLOUR`: Well, duh!
- `ENVELOPE`: There is only a beeper, so not much scope for this
- `SOUND`: I may implement this at some point
- `ADVAL`: There are no peripherals yet
- `TIME$`: And no real-time clock

