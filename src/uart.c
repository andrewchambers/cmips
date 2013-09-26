#include "mips.h"

#include <stdio.h>

uint32_t uart_read(Mips * emu,uint32_t offset){
    return 0;
}


void uart_write(Mips * emu,uint32_t offset,uint32_t v){
    
}

uint8_t uart_readb(Mips * emu,uint32_t offset){
    return 0x60;
}


void uart_writeb(Mips * emu,uint32_t offset,uint8_t v){
    putchar(v);
    fflush(stdout);
}
