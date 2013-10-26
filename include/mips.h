#include <stdint.h>
#include <stdio.h>

#define UARTBASE 0x140003f8
#define UARTSIZE 20
#define POWERBASE 0x1fbf0004
#define POWERSIZE 4


typedef struct  {
    uint32_t VPN;
    uint32_t PageMask;
    uint8_t ASID;
    uint16_t G:1;
    uint32_t C0:3;
    uint32_t C1:3;
    uint32_t V0:1;
    uint32_t V1:1;
    uint32_t D0:1;
    uint32_t D1:1;
    uint32_t PFN[2];
} TLB_entry;

typedef struct {
    TLB_entry entries[16];
    int exceptionWasNoMatch;
} TLB;


typedef struct {
    uint8_t LCR; // Line Control, reset, character has 8 bits
    uint8_t LSR; // Line Status register, Transmitter serial register empty and Transmitter buffer register empty
    uint8_t MSR; // modem status register
    uint8_t IIR; // Interrupt Identification, no interrupt
    uint8_t ints;// no interrupt pending
    uint8_t IER; //Interrupt Enable
    uint8_t DLL;
    uint8_t DLH;
    uint8_t FCR; // FIFO Control;
    uint8_t MCR; // Modem Control
    uint8_t input;
    
    uint8_t fifo[32]; //ring buffer
    uint32_t fifoIdx;
    uint32_t fifoCur;
    
} Uart;



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
    uint8_t exceptionOccured;
    
    
    int llbit;
    uint32_t CP0_Index;
    uint32_t CP0_EntryHi;
    uint32_t CP0_EntryLo0;
    uint32_t CP0_EntryLo1;
    uint32_t CP0_Context;
    uint32_t CP0_Wired;
    uint32_t CP0_Status;
    uint32_t CP0_Epc;
    uint32_t CP0_BadVAddr;
    uint32_t CP0_ErrorEpc;
    uint32_t CP0_Cause;
    uint32_t CP0_PageMask;
    
    uint32_t CP0_Count;
    uint32_t CP0_Compare;
    
    Uart serial;
    
    TLB tlb;
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
int loadSrecFromString_mips(Mips * emu,char * srec);

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



void uart_Reset(Uart * serial);

uint32_t uart_read(Mips * emu,uint32_t offset);
void uart_write(Mips * emu,uint32_t offset,uint32_t v);
uint8_t uart_readb(Mips * emu,uint32_t offset);
void uart_writeb(Mips * emu,uint32_t offset,uint8_t v);

extern char * regn2o32[];

