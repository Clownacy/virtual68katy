# Virtual 68 Katy

This is an emulator for the 68 Katy - a hand-built Motorola 68000-based
computer that runs Linux.

Additional information about the 68 Katy can be found here, along with software
and schematics:
https://www.bigmessowires.com/68-katy/

This emulator is written in ANSI C, licensed under the AGPLv3, and relies on
either WinAPI or POSIX-2001 for threading and terminal manipulation.

The 68 Katy comes in two binary-incompatible variants: breadboard and PCB.
Virtual 68 Katy emulates the PCB version by default, but it can be made to
emulate the breadboard version by passing the `-b` flag.

This emulator runs entirely from the command line, and can be exited by
pressing the escape key twice. When attempting to boot Linux, the 68 Katy will
first enter a monitor program; to enter Linux, enter the command 'j003000'
(jump to address 0x3000, where Linux's bootloader begins).
