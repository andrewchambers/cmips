#include <stdint.h>
#include <stdio.h>

#define UARTBASE 0x140003f8
#define UARTSIZE 20
#define POWERBASE 0x1fbf0004
#define POWERSIZE 4

typedef struct {
    uint32_t * mem;
    uint32_t pmemsz;
    uint32_t shutdown;
    uint32_t pc;
    uint32_t regs[32];
    uint32_t hi;
    uint32_t lo;
    uint32_t delaypc;
    uint8_t inDelaySlot;
} Mips;

Mips * new_mips(uint32_t physMemSize);
void free_mips(Mips * mips);
void step_mips(Mips * emu);

typedef struct {
    void * userdata;
    int (*nextChar)(void *);
    int (*isEof)(void *);
} SrecLoader;

int loadSrec_mips(Mips * emu,SrecLoader * s);
int loadSrecFromFile_mips(Mips * emu,char * fname);

typedef struct {
    FILE * file;
    uint32_t regs[32];
    uint32_t hi;
    uint32_t lo;
    uint32_t pc;
    uint32_t canary; // memory canary used to trace writes
} EmuTrace;

EmuTrace * startTrace(char * tracefile);
void updateTrace(EmuTrace * t,Mips * emu);
void endTrace(EmuTrace * t);

uint32_t uart_read(Mips * emu,uint32_t offset);
void uart_write(Mips * emu,uint32_t offset,uint32_t v);
uint8_t uart_readb(Mips * emu,uint32_t offset);
void uart_writeb(Mips * emu,uint32_t offset,uint8_t v);

extern char * regn2o32[];

