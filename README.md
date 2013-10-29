cmips
=====

Modular mips 4kc emulator designed to be embedded inside other applications.

Currently supports a uart chip, and uses the cpu timer.

will eventually support pluggable framebuffer, mtd flash, 8250 uart, real time clock.

Status Currently partially boots a custom built linux kernel. Booting fails after reaching 
/sbin/init userspace.

test suite located at https://github.com/andrewchambers/met.

