cmips
=====

Tiny mips 4kc emulator (smallest and easiest emulator to hack on that I know of - It boots linux in a few thousands lines of C code.)
I wrote this emulator in the month or so after I graduated university and before I started work.


Motivations
===========

1. To test my skills and learn about processors and operating systems.
2. To compile to javascript (asm.js) with emscripten to test it vs Fabrice Bellard's http://bellard.org/jslinux/ (I started this, but didnt finish writing a terminal emulator.)
3. To embed in other applications like games, simulators and sandboxes so people can have a realistic processor to play with.  It doesn't really have any dependencies,
   so if you rewrite the serial port code, and embed the step function in your program loop somehow, it will work very easily.

Status
======

Currently boots a custom built linux kernel. timers don't work quite right,
so calculating bogomips takes a while during boot, and time readings are off.
It currently supports a uart chip, and uses the cpu timer.
It may eventually support pluggable framebuffer, mtd flash, 8250 uart, real time clock if there are reasons to extend it.
It uses the linux emulation of floating point instructions.

(Pull requests welcome.)

Usage
=====
I have not put my custom linux kernel board support code online yet, so I included a precompiled kernel in the images folder.

Tests
=====
test suite I used for bootstrapping located at https://github.com/andrewchambers/met.
The linux kernel itself is more comprehensive.

I used creduce and csmith as a way of fuzzing my emulator initially too.

Misc
====

The disgen.py program is just a python script which converts a json representation of the opcodes
into a giant switch case for disassembling and executing.

Info
====

Those who wish to try similar projects, I learnt most of this by working with my friend on a predecessor javascript emulator.
I mainly used the mips 4kc manual I got from wikipedia, and I read parts of the qemu code if things were not totally clear.

The uart code is inspired from http://s-macke.github.io/jor1k/. 

The people on #linux-mips also helped me when I hit a bug in the mips linux kernel which prevented booting.
