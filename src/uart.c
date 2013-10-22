#include "mips.h"

#include <stdio.h>

//XXX remove this when exit calls are removed...
#include <stdlib.h>

//XXX ThrowCTI NextInterrupt and ClearInterrupt recurse sometimes
//seemingly pointlessly.

/* some forward declarations */

static void uart_NextInterrupt(Uart * serial); 
static uint8_t uart_ReadReg8(Uart * serial ,uint32_t offset);
static void uart_WriteReg8(Uart * serial,uint32_t offset, uint8_t x);




uint32_t uart_read(Mips * emu,uint32_t offset){
    return 0;
}


void uart_write(Mips * emu,uint32_t offset,uint32_t v){
    
}

uint8_t uart_readb(Mips * emu,uint32_t offset){
    uint8_t ret = uart_ReadReg8(&(emu->serial),offset);
    //printf("serial readb %u %x\n",offset, ret);
    return ret;
}


void uart_writeb(Mips * emu,uint32_t offset,uint8_t v){
    //printf("serial writeb %u %c\n",offset,v);
    uart_WriteReg8(&(emu->serial),offset,v);
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

static int uart_fifoHasData(Uart * serial) {
    return serial->fifoCur != serial->fifoIdx;
}

static void uart_fifoPush(Uart * serial, uint8_t c) {
    serial->fifo[serial->fifoIdx] = c;
    serial->fifoIdx = (serial->fifoIdx + 1) % 32;
    if(!uart_fifoHasData(serial)) {
        //XXX handle this better
        puts("uart overflow!");
        exit(1);
    }
}

static uint8_t uart_fifoGet(Uart * serial) {
    uint8_t c = serial->fifo[serial->fifoCur];
    serial->fifoCur = (serial->fifoCur + 1) % 32;
    return c;
}

static void uart_fifoClear(Uart * serial) {
    serial->fifoCur = 0;
    serial->fifoIdx = 0;
}


/* end Fifo code */


void uart_Reset(Uart * serial) {
    serial->LCR = 3;
    serial->LSR = UART_LSR_TRANSMITTER_EMPTY | UART_LSR_FIFO_EMPTY;
    serial->MSR = 0;
    serial->IIR = UART_IIR_NO_INT;
    serial->ints = 0;
    serial->IER = 0;
    serial->DLL = 0;
    serial->DLH = 0;
    serial->FCR = 0;
    serial->MCR = 0;
    serial->input = 0; //current input char
    
    uart_fifoClear(serial);
}

static void uart_ThrowCTI (Uart * serial){
    serial->ints |= 1 << UART_IIR_CTI;
    if (!(serial->IER & UART_IER_RDI)) {
        return;
    }
    if ((serial->IIR != UART_IIR_RLSI) && (serial->IIR != UART_IIR_RDI)) {
        serial->IIR = UART_IIR_CTI;
        puts("unimplemented raise interrupt");
        exit(1);
        //RaiseInterrupt(0x2);
    }
};


//Internal uart interrupts not the external interrupt line
static void uart_ClearInterrupt(Uart * serial,int line) {
    serial->ints &= ~ (1 << line);
    serial->IIR = UART_IIR_NO_INT;
    if (line != serial->IIR) {
        return;
    }
    uart_NextInterrupt(serial);
};

static void uart_ThrowTHRI (Uart * serial){
    serial->ints |= 1 << UART_IIR_THRI;
    if (!(serial->IER & UART_IER_THRI)) {
        return;
    }
    if ((serial->IIR & UART_IIR_NO_INT) || (serial->IIR == UART_IIR_MSI) || (serial->IIR == UART_IIR_THRI)) {
        serial->IIR = UART_IIR_THRI;
        puts("unimplemented raise interrupt");
        exit(1);
        //RaiseInterrupt(0x2);
    }
};

static void uart_RecieveChar(Uart *serial, uint8_t c) {
    uart_fifoPush(serial,c);
    serial->input = uart_fifoGet(serial);
    uart_ClearInterrupt(serial,UART_IIR_CTI);
    serial->LSR |= UART_LSR_DATA_READY;
    uart_ThrowCTI(serial);
};

static void uart_NextInterrupt(Uart * serial) {
    if ((serial->ints & (1 << UART_IIR_CTI)) && (serial->IER & UART_IER_RDI)) {
        uart_ThrowCTI(serial);
    } else if ((serial->ints & (1 << UART_IIR_THRI)) && (serial->IER & UART_IER_THRI)) {
        uart_ThrowTHRI(serial);
    } else {
        serial->IIR = UART_IIR_NO_INT;
        puts("unimplemented clear interrupt");
        exit(1);
        //ClearInterrupt(0x2);
    }
};


static uint8_t uart_ReadReg8(Uart * serial ,uint32_t offset) {
    
    uint8_t ret;
    
    offset &= 7;
    
    if (serial->LCR & UART_LCR_DLAB) {
        switch (offset) {
            case UART_DLL:
                return serial->DLL;
            case UART_DLH:
                return serial->DLH;
        }
    }
    switch (offset) {
    case 0:
        serial->input = 0;
        uart_ClearInterrupt(serial,UART_IIR_RDI);
        uart_ClearInterrupt(serial,UART_IIR_CTI);
        if (uart_fifoHasData(serial)) {
            // not sure if this is right, probably not
            serial->input = uart_fifoGet(serial);
            serial->LSR |= UART_LSR_DATA_READY;
            //uart_ThrowCTI(uart);
        }
        else {
            serial->LSR &= ~UART_LSR_DATA_READY;
        }
        return ret;
    case UART_IER:
        return serial->IER & 0x0F;
    case UART_MSR:
        return serial->MSR;
    case UART_IIR:
        ret = (serial->IIR & 0x0f) | 0xC0; // the two top bits are always set
        if (serial->IIR == UART_IIR_THRI) {
            uart_ClearInterrupt(serial,UART_IIR_THRI);
        }
        return ret;
    case UART_LCR:
        return serial->LCR;
    case UART_LSR:
        return serial->LSR;
    default:
        printf("Error in uart ReadRegister: not supported %u\n",offset);
        exit(1);
    }
};

static void uart_WriteReg8(Uart * serial,uint32_t offset, uint8_t x) {
    
    offset &= 7;
    
    if (serial->LCR & UART_LCR_DLAB) {
        switch (offset) {
        case UART_DLL:
            serial->DLL = x;
            return;
        case UART_DLH:
            serial->DLH = x;
            return;
        }
    }

    switch (offset) {
    case 0:
        serial->LSR &= ~UART_LSR_FIFO_EMPTY;
        
        putchar(x);
        fflush(stdout);
        
        // Data is send with a latency of zero!
        serial->LSR |= UART_LSR_FIFO_EMPTY; // send buffer is empty					
        uart_ThrowTHRI(serial);
        return;
    case UART_IER:
        // 2 = 10b ,5=101b, 7=111b
        serial->IER = x & 0x0F; // only the first four bits are valid
        // Ok, check immediately if there is a interrupt pending
        uart_NextInterrupt(serial);
        break;
    case UART_FCR:
        serial->FCR = x;
        if (serial->FCR & 2) {
            uart_fifoClear(serial);
        }
        break;
    case UART_LCR:
        serial->LCR = x;
        break;
    case UART_MCR:
        serial->MCR = x;
        break;
    default:
        printf("Error in uart WriteRegister: not supported %u\n",offset);
        exit(1);
    }
};

