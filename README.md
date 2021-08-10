# cerberus-bbc-basic

A port of BBC Basic for Z80 to the Cerberus 2080

### What is the Cerberus 2080

The Cerberus 2080 is an innovative 8-bit multi-processor microcomputer board designed by Bernardo Kastrup aka The Byte Attic. It can run either Z80 or 6502 code off native on-board CPUs, and the board BIOS runs off an ATmega328p. The board is open-source, and you can find more details about it here.

https://www.thebyteattic.com/p/cerberus-2080.html

### What is BBC Basic for Z80?

The original version of BBC Basic was written by Sophie Wilson at Acorn in 1981 for the BBC Micro range of computers, and was designed to support the UK Computer Literacy Project. R.T.Russell was involved in the specification of BBC Basic, and wrote his own Z80 version that was subsequently ported to a number of Z80 based machines. [I highly recommend reading his account of this on his website for more details](http://www.bbcbasic.co.uk/bbcbasic/history.html).

As an aside, R.T.Russell still supports BBC Basic, and has ported it for a number of modern platforms, including Android, Windows, and SDL, which are [available from his website here](https://www.bbcbasic.co.uk/index.html).

### Why am I doing this?

Bernardo was originally looking for someone to port GW BASIC to the Cerberus, and my name was suggested, given my previous experience porting BBC BASIC to the BSX and the Spectrum Next. I initially declined, until someone suggested to Bernardo that it may be a better idea to port BBC Basic for Z80 to the Cerberus. He agreed, I accepted, and the rest as they say is history.

### The Challenges

The foundation of this port is R.T.Russell's original BBC Basic for Z80 code (for CP/M), with the CP/M specific code stripped out. This foundation code is just pure BASIC interpreter with no graphics, sound or file I/O support.

The main challenge will be extending the BIOS of the Cerberus to support BASIC language features such as sound and file I/O.

### Assembling

The code is written to be assembled by the SJASMPLUS assembler. Details of the toolchain I use [can be found here on my website](http://www.breakintoprogram.co.uk/computers/zx-spectrum-next/assembly-language/z80-development-toolchain).

Every endeavour will be made to ensure the code is stable and will assemble, yet as this is a work-in-progress done in my spare time there will be instances where this is not the case.

> Please avoid raising pull requests: The code will be in a state of flux for at least the next couple of months. I'm not quite geared up for collaboration just yet!

> Raise an issue if you notice anything amiss; I will endeavour to get back to you. Bear with me if it takes a while to get back to you.

### Running

This code currently requires a slightly customised BIOS. This can be found in the [cat](cat) folder, along with instructions on building and installing it.

### Releases

I'll dump significant updates as executable bin files in the [releases](releases) folder

### License

This code is distributable under the terms of a zlib license. Read the file [COPYING](COPYING) for more information.

The code, as originally written by R.T. Russell and [downloaded from David Given's GitHub page](https://github.com/davidgiven/cpmish/tree/master/third_party/bbcbasic), has been modified slightly, either for compatibility reasons when assembling using sjasmplus, or for development reasons for this release:

The original files are: [eval.z80](eval.z80), [exec.z80](exec.z80), [fpp.z80](fpp.z80), [patch.z80](patch.z80), [ram.z80](ram.z80) and [sorry.z80](sorry.z80).

- General changes:
	- The top-of-file comments have been tweaked to match my style
	- GLOBAL and EXPORT directives have been removed and any global labels prefixed with @
	- Source in z80 now enclosed in MODULES to prevent label clash
	- A handful of '"' values have been converted to 34, and commented with ASCII '"'
	- A [build.z80](build.z80) file has been added; this includes all other files and is the file to build
	- All CPMish code has been removed as this version is not going to run on CP/M
- File [patch.z80](patch.z80)
	- The function GET has been moved into it from [eval.z80](eval.z80)

Other than that, the source code is equivalent to the code originally authored by R.T.Russell, downloaded on David Given's website: 

http://cowlark.com/2019-06-14-bbcbasic-opensource/index.html

The bulk of the Cerberus specific code I've written can be found in the z80 files prefixed with "cerberus_". I've clearly commented any changes made to R.T.Russell's original source files in the source code. In addition to the above, I've added the following files: [editor.z80](editor.z80) and [misc.z80](misc.z80)

Any additions or modifications I've made to port this to the Cerberus 2080 have been released under the same licensing terms as the original code, along with any tools, examples or utilities contained within this project. Code that has been copied or inspired by other sources is clearly marked, with the appropriate accreditations.

Dean Belfield

Twitter: [@breakintoprogram](https://twitter.com/BreakIntoProg)
Blog: http://www.breakintoprogram.co.uk
