#include "mips.h"

#include <stdio.h>

//XXX remove this when exit calls are removed...
#include <stdlib.h>

//XXX ThrowCTI NextInterrupt and ClearInterrupt recurse sometimes
//seemingly pointlessly.

/* some forward declarations */

static void uart_NextInterrupt(Mips * emu); 
static uint8_t uart_ReadReg8(Mips * emu ,uint32_t offset);
static void uart_WriteReg8(Mips * emu,uint32_t offset, uint8_t x);




uint32_t uart_read(Mips * emu,uint32_t offset){
    return 0;
}


void uart_write(Mips * emu,uint32_t offset,uint32_t v){
    
}

uint8_t uart_readb(Mips * emu,uint32_t offset){
    uint8_t ret = uart_ReadReg8(emu,offset);
    //printf("serial readb %u %x\n",offset, ret);
    return ret;
}


void uart_writeb(Mips * emu,uint32_t offset,uint8_t v){
    //printf("serial writeb %u %c\n",offset,v);
    uart_WriteReg8(emu,offset,v);
}


// code inspired by/ported from
// https://raw.github.com/s-macke/jor1k/master/js/worker/uart.js
// -------------------------------------------------
// -------------------- UART -----------------------
// -------------------------------------------------

#define UART_LSR_DATA_READY 0x1
#define UART_LSR_FIFO_EMPTY 0x20
#define UART_LSR_TRANSMITTER_EMPTY 0x40

#define UART_IER_THRI 0x02 /* Enable Transmitter holding register int. */
#define UART_IER_RDI 0x01 /* Enable receiver data interrupt */

#define UART_IIR_MSI 0x00 /* Modem status interrupt (Low priority) */
#define UART_IIR_NO_INT 0x01
#define UART_IIR_THRI 0x02 /* Transmitter holding register empty */
#define UART_IIR_RDI 0x04 /* Receiver data interrupt */
#define UART_IIR_RLSI 0x06 /* Receiver line status interrupt (High p.) */
#define UART_IIR_CTI 0x0c /* Character timeout */

#define UART_LCR_DLAB 0x80 /* Divisor latch access bit */

#define UART_DLL 0 /* R/W: Divisor Latch Low, DLAB=1 */
#define UART_DLH 1 /* R/W: Divisor Latch High, DLAB=1 */

#define UART_IER 1 /* R/W: Interrupt Enable Register */
#define UART_IIR 2 /* R: Interrupt ID Register */
#define UART_FCR 2 /* W: FIFO Control Register */
#define UART_LCR 3 /* R/W: Line Control Register */
#define UART_MCR 4 /* W: Modem Control Register */
#define UART_LSR 5 /* R: Line Status Register */
#define UART_MSR 6 /* R: Modem Status Register */

/* FIFO code - used so we dont drop characters of input */
// dont call these directly, 

static int uart_fifoHasData(Mips * emu) {
    return emu->serial.fifoCount > 0;
}

static void uart_fifoPush(Mips * emu, uint8_t c) {
    emu->serial.fifo[emu->serial.fifoLast] = c;
    emu->serial.fifoLast = (emu->serial.fifoLast + 1) % 32;
    emu->serial.fifoCount++;
    if(emu->serial.fifoCount > 32) {
       emu->serial.fifoCount = 32; 
    }
}

static uint8_t uart_fifoGet(Mips * emu) {
    
    uint8_t c;
    
    if(!uart_fifoHasData(emu)) {
        return 0;
    }
    
    c = emu->serial.fifo[emu->serial.fifoFirst];
    emu->serial.fifoFirst = (emu->serial.fifoFirst + 1) % 32;
    
    if(emu->serial.fifoCount) {
        emu->serial.fifoCount--;
    }
    
    return c;
}

static void uart_fifoClear(Mips * emu) {
    emu->serial.fifoLast = 0;
    emu->serial.fifoFirst = 0;
    emu->serial.fifoCount = 0;
}


/* end Fifo code */


void uart_Reset(Mips * emu) {
    emu->serial.LCR = 3;
    emu->serial.LSR = UART_LSR_TRANSMITTER_EMPTY | UART_LSR_FIFO_EMPTY;
    emu->serial.MSR = 0;
    emu->serial.IIR = UART_IIR_NO_INT;
    emu->serial.ints = 0;
    emu->serial.IER = 0;
    emu->serial.DLL = 0;
    emu->serial.DLH = 0;
    emu->serial.FCR = 0;
    emu->serial.MCR = 0;
    
    uart_fifoClear(emu);
}

static void uart_ThrowCTI (Mips * emu){
    emu->serial.ints |= 1 << UART_IIR_CTI;
    if (!(emu->serial.IER & UART_IER_RDI)) {
        return;
    }
    if ((emu->serial.IIR != UART_IIR_RLSI) && (emu->serial.IIR != UART_IIR_RDI)) {
        emu->serial.IIR = UART_IIR_CTI;
        puts("unimplemented raise interrupt");
        exit(1);
        //RaiseInterrupt(0x2);
    }
};


//Internal uart interrupts not the external interrupt line
static void uart_ClearInterrupt(Mips * emu,int line) {
    emu->serial.ints &= ~ (1 << line);
    emu->serial.IIR = UART_IIR_NO_INT;
    if (line != emu->serial.IIR) {
        return;
    }
    uart_NextInterrupt(emu);
};

static void uart_ThrowTHRI (Mips * emu){
    emu->serial.ints |= 1 << UART_IIR_THRI;
    if (!(emu->serial.IER & UART_IER_THRI)) {
        return;
    }
    if ((emu->serial.IIR & UART_IIR_NO_INT) || (emu->serial.IIR == UART_IIR_MSI) || (emu->serial.IIR == UART_IIR_THRI)) {
        emu->serial.IIR = UART_IIR_THRI;
        triggerExternalInterrupt(emu,0);
    }
};

static void uart_RecieveChar(Mips * emu, uint8_t c) {
    uart_fifoPush(emu,c);
    uart_ClearInterrupt(emu,UART_IIR_CTI);
    uart_ThrowCTI(emu);
};

static void uart_NextInterrupt(Mips * emu) {
    if ((emu->serial.ints & (1 << UART_IIR_CTI)) && (emu->serial.IER & UART_IER_RDI)) {
        uart_ThrowCTI(emu);
    } else if ((emu->serial.ints & (1 << UART_IIR_THRI)) && (emu->serial.IER & UART_IER_THRI)) {
        uart_ThrowTHRI(emu);
    } else {
        emu->serial.IIR = UART_IIR_NO_INT;
        clearExternalInterrupt(emu,0);
    }
};


static uint8_t uart_ReadReg8(Mips * emu ,uint32_t offset) {
    
    uint8_t ret;
    
    offset &= 7;
    
    if (emu->serial.LCR & UART_LCR_DLAB) {
        switch (offset) {
            case UART_DLL:
                return emu->serial.DLL;
            case UART_DLH:
                return emu->serial.DLH;
        }
    }
    switch (offset) {
    case 0:
        ret = 0;
        uart_ClearInterrupt(emu,UART_IIR_RDI);
        uart_ClearInterrupt(emu,UART_IIR_CTI);
        if (uart_fifoHasData(emu)) {
            emu->serial.LSR |= UART_LSR_DATA_READY;
            //uart_ThrowCTI(uart);
            ret = uart_fifoGet(emu);
        }

        return ret;
    case UART_IER:
        return emu->serial.IER & 0x0F;
    case UART_MSR:
        return emu->serial.MSR;
    case UART_MCR:
        return emu->serial.MCR;
    case UART_IIR:
        ret = (emu->serial.IIR & 0x0f) | 0xC0; // the two top bits are always set
        if (emu->serial.IIR == UART_IIR_THRI) {
            uart_ClearInterrupt(emu,UART_IIR_THRI);
        }
        return ret;
    case UART_LCR:
        return emu->serial.LCR;
    case UART_LSR:
        if (uart_fifoHasData(emu)) {
            emu->serial.LSR |= UART_LSR_DATA_READY;
        }
        else {
            emu->serial.LSR &= ~UART_LSR_DATA_READY;
        }
        return emu->serial.LSR;
    default:
        printf("Error in uart ReadRegister: not supported %u\n",offset);
        exit(1);
    }
};

static void uart_WriteReg8(Mips * emu,uint32_t offset, uint8_t x) {
    
    offset &= 7;
    
    if (emu->serial.LCR & UART_LCR_DLAB) {
        switch (offset) {
        case UART_DLL:
            emu->serial.DLL = x;
            return;
        case UART_DLH:
            emu->serial.DLH = x;
            return;
        }
    }

    switch (offset) {
    case 0:
        emu->serial.LSR &= ~UART_LSR_FIFO_EMPTY;
        
        if (emu->serial.MCR & (1 << 4)) { //LOOPBACK 
            uart_RecieveChar(emu,x);
        } else {
            putchar(x);
            fflush(stdout);
        }
        
        
        // Data is send with a latency of zero!
        emu->serial.LSR |= UART_LSR_FIFO_EMPTY; // send buffer is empty					
        uart_ThrowTHRI(emu);
        return;
    case UART_IER:
        // 2 = 10b ,5=101b, 7=111b
        emu->serial.IER = x & 0x0F; // only the first four bits are valid
        // Ok, check immediately if there is a interrupt pending
        uart_NextInterrupt(emu);
        break;
    case UART_FCR:
        emu->serial.FCR = x;
        if (emu->serial.FCR & 2) {
            uart_fifoClear(emu);
        }
        break;
    case UART_LCR:
        emu->serial.LCR = x;
        break;
    case UART_MCR:
        emu->serial.MCR = x;
        break;
    default:
        printf("Error in uart WriteRegister: not supported %u\n",offset);
        exit(1);
    }
};

