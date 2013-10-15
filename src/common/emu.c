
#include "mips.h"
#include <stdlib.h>
#include <stdio.h>


#define CP0St_CU3   31
#define CP0St_CU2   30
#define CP0St_CU1   29
#define CP0St_CU0   28
#define CP0St_RP    27
#define CP0St_FR    26
#define CP0St_RE    25
#define CP0St_MX    24
#define CP0St_PX    23
#define CP0St_BEV   22
#define CP0St_TS    21
#define CP0St_SR    20
#define CP0St_NMI   19
#define CP0St_IM    8
#define CP0St_KX    7
#define CP0St_SX    6
#define CP0St_UX    5
#define CP0St_KSU   3
#define CP0St_ERL   2
#define CP0St_EXL   1
#define CP0St_IE    0


char * regn2o32[] = { 
                        "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
                        "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
                        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
                        "t8", "t9", "k0", "k1", "gp", "sp", "s8","ra" 
                    };


void doop(Mips * emu, uint32_t op); // predeclare doop its only local to this file


Mips * new_mips(uint32_t physMemSize) {
    
    if (physMemSize % 4 != 0) {
        return 0;
    }
    
    void * mem = calloc(physMemSize,1);
    
    if(!mem) {
        return 0;
    }
    
    Mips * ret = calloc(1,sizeof(Mips));
    
    if (!ret) {
        free(mem);
        return 0;
    }
    
    ret->mem = mem;
    ret->pmemsz = physMemSize;
    
    // Init status registers
    
    ret->CP0_Status |= (1 << CP0St_ERL); //start in kernel mode with unmapped useg
    
    return ret;
}

void free_mips(Mips * mips) {
    free(mips->mem);
    free(mips);
}

/* bitwise helpers */

int32_t sext18(uint32_t val) {
	if ( (val&0x20000) != 0 ) {
		return 0xfffc0000 | val;
	}
	return 0x0003ffff&val;
}



//Possible Values for the EXC field in the status reg
#define EXC_Int 0
#define EXC_Mod 1
#define EXC_TLBL 2
#define EXC_TLBS 3
#define EXC_AdEL 4
#define EXC_AdES 5
#define EXC_IBE 6
#define EXC_DBE 7
#define EXC_SYS 8
#define EXC_BP 9
#define EXC_RI 10
#define EXC_CpU 11
#define EXC_Ov 12
#define EXC_Tr 13
#define EXC_Watch 23
#define EXC_MCheck 24

static inline uint32_t getExceptionCode(Mips * emu) {
    return (emu->CP0_Cause >> 2) & 0x1f;
}

static inline void setExceptionCode(Mips * emu,int code) {
    emu->CP0_Cause &= ~(0x1f << 2); // clear exccode
    emu->CP0_Cause |= (code & 0x1f) << 2; //set with new code
}


//tlb lookup return codes

#define TLBRET_MATCH 0
#define TLBRET_NOMATCH 1
#define TLBRET_DIRTY 2
#define TLBRET_INVALID 3


// tlb code modified from qemu


static int tlb_lookup (Mips *emu,uint32_t vaddress, uint32_t *physical, int write) {
    uint8_t ASID = emu->CP0_EntryHi & 0xFF;
    int i;
    //printf("tlblookup for addr %08x\n",vaddress);
    for (i = 0; i < 16; i++) {
        //printf("ent %d\n",i);
        TLB_entry *tlb_e = &emu->tlb.entries[i];
        /* 1k pages are not supported. */
        uint32_t mask = tlb_e->PageMask;
        uint32_t tag = vaddress & ~mask;
        uint32_t VPN = tlb_e->VPN & ~mask;
        //printf("mask: %08x tag: %08x vpn %08x\n",mask,tag,VPN);
        /* Check ASID, virtual page number & size */
        if ((tlb_e->G == 1 || tlb_e->ASID == ASID) && VPN == tag) {
            /* TLB match */
            int n = !!(vaddress & mask & ~(mask >> 1));
            //printf("match! %d \n",n);
            /* Check access rights */
            if (!(n ? tlb_e->V1 : tlb_e->V0)) {
                //printf("invalid %d %d\n",tlb_e->V1,tlb_e->V0);
                emu->exceptionOccured = 1;
                setExceptionCode(emu,write ? EXC_TLBS : EXC_TLBL);
                return TLBRET_INVALID;
            }
            if (write == 0 || (n ? tlb_e->D1 : tlb_e->D0)) {
                //printf("\n returning %08x\n",tlb_e->PFN[n] | (vaddress & (mask >> 1)));
                *physical = tlb_e->PFN[n] | (vaddress & (mask >> 1));
                return TLBRET_MATCH;
            }
            emu->exceptionOccured = 1;
            setExceptionCode(emu,write ? EXC_TLBS : EXC_TLBL);
            return TLBRET_DIRTY;
        }
    }
    emu->exceptionOccured = 1;
    setExceptionCode(emu,write ? EXC_TLBS : EXC_TLBL);
    return TLBRET_NOMATCH;
}

//internally triggers exceptions on error
//returns an error code
static inline int translateAddress(Mips * emu,uint32_t vaddr,uint32_t * paddr_out,int write) {
    
    if (vaddr <= (int32_t)0x7FFFFFFFUL) {
        /* useg */
        if (emu->CP0_Status & (1 << CP0St_ERL)) {
            *paddr_out = vaddr;
            return 0;
        } else {
            return tlb_lookup(emu,vaddr,paddr_out, write);
        }
    } else if ( vaddr >= 0x80000000 && vaddr <= 0x9fffffff ) {
        *paddr_out = vaddr - 0x80000000;
        return 0;
    } else if ( vaddr >= 0xa0000000 && vaddr <= 0xbfffffff ) {
        *paddr_out = vaddr - 0xa0000000;
        return 0;
    } else {
        *paddr_out = vaddr;
        return 0;
    }
    
}



static uint32_t readVirtWord(Mips * emu, uint32_t addr) {
    uint32_t paddr;
    int err = translateAddress(emu,addr,&paddr,0);
    if(err) {
        return 0;
    }
    
    if(paddr % 4 != 0) {
        printf("Unhandled alignment error reading addr %08x\n",addr);
        exit(1); 
    }
    
    if(paddr >= UARTBASE && paddr <= UARTBASE + UARTSIZE) {
        return uart_read(emu,paddr - UARTBASE);
    }
    
    if (paddr >= emu->pmemsz) {
        printf("unhandled bus error paddr: %08x\n",paddr);
        exit(1);
    }
    
    return emu->mem[paddr/4];    
}

static void writeVirtWord(Mips * emu, uint32_t addr,uint32_t val) {
    uint32_t paddr;
    int err = translateAddress(emu,addr,&paddr,1);
    if(err) {
        return;
    }
    
    if(paddr % 4 != 0) {
        printf("Unhandled alignment error reading addr %08x\n",addr);
        exit(1); 
    }
    
    if(paddr >= UARTBASE && paddr <= UARTBASE + UARTSIZE) {
        uart_write(emu,paddr - UARTBASE,val);
        return;
    }
    
    if (paddr >= emu->pmemsz) {
        printf("unhandled bus error paddr: %08x\n",paddr);
        exit(1);
    }
    
    emu->mem[paddr/4] = val;    
}

static uint8_t readVirtByte(Mips * emu, uint32_t addr) {
    uint32_t paddr;
    int err = translateAddress(emu,addr,&paddr,0);
    if(err) {
        return 0;
    }
    
    if(paddr >= UARTBASE && paddr <= UARTBASE + UARTSIZE) {
        return uart_readb(emu,paddr - UARTBASE);
    }
    
    unsigned int offset = paddr&3;
    
    if (paddr >= emu->pmemsz) {
        printf("unhandled bus error paddr: %08x\n",paddr);
        exit(1);
    }
    
    uint32_t word = emu->mem[(paddr&(~0x3)) /4];
	uint32_t shamt = 8*(3 - offset);
	uint32_t mask = 0xff << shamt;
	uint8_t b = (word&mask) >> shamt;
	return b;  
}

static void writeVirtByte(Mips * emu, uint32_t addr,uint8_t val) {
    uint32_t paddr;
    
    int err = translateAddress(emu,addr,&paddr,1);
    if(err) {
        return;
    }
    
    if(paddr >= UARTBASE && paddr <= UARTBASE + UARTSIZE) {
        uart_writeb(emu,paddr - UARTBASE,val);
        return;
    }
    
    if(paddr >= POWERBASE && paddr <= POWERBASE + POWERSIZE) {
        emu->shutdown = 1;
        return;
    }
    
    if (paddr >= emu->pmemsz) {
        printf("unhandled bus error paddr: %08x\n",paddr);
        exit(1);
    }
	
	unsigned int offset = paddr&3;
	uint32_t baseaddr = paddr&(~0x3);
	uint32_t word = emu->mem[baseaddr/4];
	unsigned int shamt = 8*(3 - offset);
	uint32_t clearmask = ~(0xff<<shamt);
	uint32_t valmask = (val << shamt);
	word = (word&clearmask);
	word = (word|valmask);
	emu->mem[baseaddr/4] = word;
}

static void handleException(Mips * emu,int inDelaySlot) {

    uint32_t offset;
    int exccode = getExceptionCode(emu);
    
    if ( (emu->CP0_Status & (1 << CP0St_EXL))  == 0) {
        if (inDelaySlot) {
            emu->CP0_Epc = emu->pc - 4;
            emu->CP0_Cause |= (1 << 31); // set BD
        } else {
            emu->CP0_Epc = emu->pc;
            emu->CP0_Cause &= ~(1 << 31); // clear BD
        }
        
        if (exccode == EXC_TLBL || exccode == EXC_TLBS ) {
            offset = 0;
        } else if ( (exccode == EXC_Int) && ((emu->CP0_Cause & (1 << 23)) != 0)) {
            offset = 0x200;
        } else {
            offset = 0x180;
        }
    } else {
        offset = 0x180;
    }
    
    // Faulting coprocessor number set at fault location
    // exccode set at fault location
    emu->CP0_Status |= (1 << CP0St_EXL);
    
    if (emu->CP0_Status & (1 << CP0St_BEV)) {
        emu->pc = 0xbfc00200 + offset;
    } else {
        emu->pc = 0x80000000 + offset;
    }
    
    emu->exceptionOccured = 0;

}

static void triggerExternalInterrupt(Mips * emu,unsigned int intNum) {
    emu->CP0_Cause |= ((1 << intNum) & 0x3f ) << 10;     
}

static void clearExternalInterrupt(Mips * emu,unsigned int intNum) {
    emu->CP0_Cause &= ~(((1 << intNum) & 0x3f ) << 10);     
}

/* return 1 if interrupt occured else 0*/
static int handleInterrupts(Mips * emu) {
    
    //if interrupts disabled or ERL or EXL set
    if((emu->CP0_Status & 1) == 0 || (emu->CP0_Status & ((1 << 1) | (1 << 2))) ) {
        return 0; // interrupts disabled
    }
    
    if((emu->CP0_Cause & emu->CP0_Status &  0xfc00) == 0) {
        return 0; // no pending interrupts
    }
    
    
    setExceptionCode(emu,0);
    handleException(emu,emu->inDelaySlot);
    
    return 1;
    
}


void step_mips(Mips * emu) {
    
    if(emu->shutdown){
        return;
    }
    
    /* timer code */
    if( (emu->CP0_Status & 1) && (emu->CP0_Count > emu->CP0_Compare) ) {
        //probably not really right, but only do this if interrupts are enabled to save time.
        triggerExternalInterrupt(emu,5); // 5 is the timer int :)
    }
    
    emu->CP0_Count++;
    
    if(handleInterrupts(emu)) {
        return;
    }
    
    /* end timer code */
    
    
	int startInDelaySlot = emu->inDelaySlot;
	
	
	uint32_t opcode = readVirtWord(emu,emu->pc);
	
	if(emu->exceptionOccured) { //instruction fetch failed
	    printf("exception!!!!!!! pc: %08x\n",emu->pc);
	    exit(1);
	    handleException(emu,startInDelaySlot);
	    return;
	}
	
	
    doop(emu,opcode);
    emu->regs[0] = 0;
    
	if(emu->exceptionOccured) { //instruction failed
	    puts("exception2!!!!!!!");
	    exit(1);
	    handleException(emu,startInDelaySlot);
	    return;
	}

	if (startInDelaySlot) {
	    emu->pc = emu->delaypc;
	    emu->inDelaySlot = 0;
	    return;
	}
    
    emu->pc += 4;
}


static inline void setRt(Mips * emu,uint32_t op,uint32_t val) {
	uint32_t idx = (op&0x1f0000) >> 16;
	emu->regs[idx] = val;
}


static inline uint32_t getRt(Mips * emu,uint32_t op) {
	uint32_t idx = (op&0x1f0000) >> 16;
	return emu->regs[idx];
}

static inline void setRd(Mips * emu,uint32_t op,uint32_t val) {
	uint32_t idx = (op&0xf800) >> 11;
	emu->regs[idx] = val;
}

static inline uint32_t getRs(Mips * emu,uint32_t op) {
	uint32_t idx = (op&0x3e00000) >> 21;
	return emu->regs[idx];
}

static inline uint16_t getImm(uint32_t op) {
	return op&0xffff;
}

static inline uint32_t getShamt(uint32_t op) {
	return (op&0x7c0) >> 6;
}

/* start opcode implementations */ 


static void op_swl(Mips * emu,uint32_t op) {
    int32_t c = (int16_t)(op&0x0000ffff);
    uint32_t addr = ((int32_t)getRs(emu,op)+c);
    uint32_t rtVal = getRt(emu,op);
    uint32_t wordVal = readVirtWord(emu,addr&(~3));
    if(emu->exceptionOccured) {
        return;
    }
    uint32_t offset = addr&3;
    uint32_t result;
    
    switch(offset) {
        case 0:
            result = rtVal;
            break;
        
        case 1:
            result = (wordVal&0xff000000) | ((rtVal>>8)&0xffffff);
            break;
        
        case 2:
            result = (wordVal&0xffff0000) | ((rtVal>>16)&0xffff);
            break;
        
        case 3:
            result = (wordVal&0xffffff00) | ((rtVal>>24)&0xff);
            break;
    }
    
    writeVirtWord(emu,addr&(~3),result);
}



static void op_lwl(Mips * emu,uint32_t op) {
    int32_t c = (int16_t)(op&0x0000ffff);
    uint32_t addr = (int32_t)getRs(emu,op)+c;
    uint32_t rtVal = getRt(emu,op);
    uint32_t wordVal = readVirtWord(emu,addr&(~3));
    if(emu->exceptionOccured) {
        return;
    }
    uint32_t offset = addr % 4;
    uint32_t result;

    switch(offset) {
        case 0:
            result = wordVal;
            break;     
        case 1:
            result = ((wordVal << 8) | (rtVal & 0xff));
            break;
        case 2:
            result = ((wordVal << 16) | (rtVal & 0xffff));
            break;
        case 3:
            result = ((wordVal << 24) | (rtVal & 0xffffff));
            break;
    }
    
    setRt(emu,op,result);
}




static void op_bne(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (getRs(emu,op) != getRt(emu,op) ) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
	} else {
		emu->delaypc = emu->pc + 8;
	}
	emu->inDelaySlot = 1;
}


static void op_bnel(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (getRs(emu,op) != getRt(emu,op) ) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
		emu->inDelaySlot = 1;
	} else {
	    emu->pc += 4;
	}
}

static void op_tne(Mips * emu,uint32_t op) {
	if (getRs(emu,op) != getRt(emu,op) ) {
		puts("unhandled trap!");
		exit(1);
	}
}


static void op_andi(Mips * emu,uint32_t op) {
    uint32_t v = getRs(emu,op) & getImm(op);
    setRt(emu,op,v);
}




static void op_jal(Mips * emu,uint32_t op) {
	uint32_t pc = emu->pc;
	uint32_t top = pc&0xf0000000;
	uint32_t addr = (top|((op&0x3ffffff) << 2));
	emu->delaypc = addr;
	emu->regs[31] = pc + 8;
	emu->inDelaySlot = 1;
}




static void op_sb(Mips * emu,uint32_t op) {
	uint32_t addr = (int32_t)getRs(emu,op) + (int16_t)getImm(op);
	writeVirtByte(emu,addr,getRt(emu,op)&0xff);
}




static void op_lb(Mips * emu,uint32_t op) {
	uint32_t addr = ((int32_t)getRs(emu,op) + (int16_t)getImm(op));
	int8_t v = (int8_t)readVirtByte(emu,addr);
	if(emu->exceptionOccured) {
        return;
    }
	setRt(emu,op,(int32_t)v);
}




static void op_beq(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (getRs(emu,op) == getRt(emu,op) ) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
	} else {
		emu->delaypc = emu->pc + 8;
	}
	emu->inDelaySlot = 1;
}

static void op_beql(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (getRs(emu,op) == getRt(emu,op) ) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
		emu->inDelaySlot = 1;
	} else {
		emu->pc = emu->pc += 4;
	}
}

static void op_lbu(Mips * emu,uint32_t op) {
	uint32_t addr = ((int32_t)getRs(emu,op) + (int16_t)getImm(op));
	uint32_t v = readVirtByte(emu,addr);
	if(emu->exceptionOccured) {
        return;
    }
	setRt(emu,op,v);
}




static void op_lw(Mips * emu,uint32_t op) {
	int16_t offset = getImm(op);
	uint32_t addr = ((int32_t)getRs(emu,op) + offset);
	uint32_t v = readVirtWord(emu,addr);
	if(emu->exceptionOccured) {
        return;
    }
	setRt(emu,op,v);
}




static void op_j(Mips * emu,uint32_t op) {
	uint32_t top = emu->pc&0xf0000000;
	uint32_t addr = top|((op&0x3ffffff)*4);
	emu->delaypc = addr;
	emu->inDelaySlot = 1;
}




static void op_sh(Mips * emu,uint32_t op) {
	int16_t offset = getImm(op);
	uint32_t addr = (int32_t)getRs(emu,op) + offset;
	uint8_t vlo = getRt(emu,op)&0xff;
	uint8_t vhi = (getRt(emu,op)&0xff00)>>8;
	writeVirtByte(emu,addr,vhi);
	if(emu->exceptionOccured) {
        return;
    }
	writeVirtByte(emu,addr+1,vlo);
}




static void op_slti(Mips * emu,uint32_t op) {
    int32_t rs = getRs(emu,op);
    int32_t c = (int16_t)getImm(op);
    if ( rs < c ) { 
        setRt(emu,op,1);
    } else {
        setRt(emu,op,0);
    }
}




static void op_sltiu(Mips * emu,uint32_t op) {
    uint32_t rs = getRs(emu,op);
    uint32_t c = (uint32_t)(int32_t)(int16_t)getImm(op);
    if ( rs < c ) { 
        setRt(emu,op,1);
    } else {
        setRt(emu,op,0);
    }
}




static void op_addiu(Mips * emu,uint32_t op) {
    int32_t v = (int32_t)getRs(emu,op) + (int16_t)(getImm(op));
    setRt(emu,op,(uint32_t)v);
}




static void op_xori(Mips * emu,uint32_t op) {
    uint32_t v = getRs(emu,op) ^ getImm(op);
    setRt(emu,op,v);
}




static void op_ori(Mips * emu,uint32_t op) {
    uint32_t v = getRs(emu,op) | getImm(op);
    setRt(emu,op,v);
}




static void op_swr(Mips * emu,uint32_t op) {
    int32_t c = (int16_t)(op&0x0000ffff);
    uint32_t addr = (int32_t)getRs(emu,op)+c;
    uint32_t rtVal = getRt(emu,op);
    uint32_t wordVal = readVirtWord(emu,addr&(~3));
    if(emu->exceptionOccured) {
        return;
    }
    uint32_t offset = addr&3;
    uint32_t result;
    
    switch(offset) {
        case 3:
            result = rtVal;
            break;
        
        case 2:
            result = ((rtVal<<8) & 0xffffff00) | (wordVal&0xff);
            break;
        
        case 1:
            result = ((rtVal<<16) & 0xffff0000) | (wordVal&0xffff);
            break;
        
        case 0:
            result = ((rtVal<<24) & 0xff000000) | (wordVal&0xffffff);
            break;
    }
    writeVirtWord(emu,addr&(~3),result);
}




static void op_sw(Mips * emu,uint32_t op) {
	int16_t offset = getImm(op);
	uint32_t addr = ((int32_t)getRs(emu,op) + offset);
	writeVirtWord(emu,addr,getRt(emu,op)); 
}




static void op_lh(Mips * emu,uint32_t op) {
	uint32_t addr = (int32_t)getRs(emu,op) + (int16_t)getImm(op);
	uint8_t vlo = readVirtByte(emu,addr+1);
	if(emu->exceptionOccured) {
        return;
    }
	uint8_t vhi = readVirtByte(emu,addr);
	if(emu->exceptionOccured) {
        return;
    }
	uint32_t v = (int32_t)(int16_t)((vhi<<8)| vlo);
	setRt(emu,op,v);
}




static void op_lui(Mips * emu,uint32_t op) {
	uint32_t v = getImm(op) << 16;
	setRt(emu,op,v);
}




static void op_addi(Mips * emu,uint32_t op) {
    uint32_t v = (int32_t)getRs(emu,op) + (int16_t)getImm(op);
    setRt(emu,op,v);
}




static void op_lhu(Mips * emu,uint32_t op) {
	uint32_t addr = (int32_t)getRs(emu,op) + (int16_t)getImm(op);
	uint8_t vlo = readVirtByte(emu,addr+1);
	if(emu->exceptionOccured) {
        return;
    }
	uint8_t vhi = readVirtByte(emu,addr);
	if(emu->exceptionOccured) {
        return;
    }
	uint32_t v = (vhi<<8)| vlo;
	setRt(emu,op,v);
}




static void op_bgtz(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) > 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
	} else {
		emu->delaypc = emu->pc + 8;
	}
	emu->inDelaySlot = 1;
}

static void op_bgtzl(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) > 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
		emu->inDelaySlot = 1;
	} else {
		emu->pc += 4;
	}
	
}



static void op_blez(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) <= 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
	} else {
		emu->delaypc = emu->pc + 8;
	}
	emu->inDelaySlot = 1;
}

static void op_blezl(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) <= 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
		emu->inDelaySlot = 1;
	} else {
		emu->pc += 4;
	}
}




static void op_lwr(Mips * emu,uint32_t op) {
    int32_t c = (int16_t)(op&0x0000ffff);
    uint32_t addr = ((int32_t)getRs(emu,op))+c;
    uint32_t rtVal = getRt(emu,op);
    uint32_t wordVal = readVirtWord(emu,addr & (~3));
    if(emu->exceptionOccured) {
        return;
    }
    uint32_t offset = addr & 3;
    uint32_t result;
    
    switch (offset) {
        case 3:
            result = wordVal;
            break;
        
        case 2:
            result = ((rtVal & 0xff000000) | (wordVal >> 8));
            break;
        
        case 1:
            result = ((rtVal & 0xffff0000) | (wordVal >> 16));
            break;
        
        case 0:
            result = ((rtVal & 0xffffff00) | (wordVal >> 24));
            break;
    }
    setRt(emu,op,result);
}

static void op_mfc0(Mips * emu,uint32_t op) {
    uint32_t regNum = (op&0xf800) >> 11;
    uint32_t sel = op & 7;
    uint32_t retval;
    switch(regNum) {
        
        case 0: // Index
            if(sel != 0) {
               goto unhandled; 
            }
            retval = emu->CP0_Index;
            break;
        case 2: //EntryLo0
            retval = emu->CP0_EntryLo0;
            break;
            
        case 3://EntryLo1
            retval = emu->CP0_EntryLo1;
            break;
            
        case 5: // Page Mask
            if(sel != 0) {
                goto unhandled;
            }
            retval = emu->CP0_PageMask;
            break;

        case 6: // Wired
            if(sel != 0) {
                goto unhandled;
            }
            retval = emu->CP0_Wired;
            break;
        
        case 9: // Count
            if(sel != 0) {
                goto unhandled;
            }
            retval = emu->CP0_Count;
            break;
            
        case 10: // EntryHi
            if (sel != 0 ) {
                goto unhandled;
            }
            retval = emu->CP0_EntryHi;
            break;

        case 11: // Compare
            if(sel != 0) {
                goto unhandled;
            }
            clearExternalInterrupt(emu,5);
            retval = emu->CP0_Compare;
            break;

        case 12: // Status
            if (sel != 0 ) {
                goto unhandled;
            }
            retval = emu->CP0_Status;
            break;
        
        case 13:
            if (sel != 0 ) {
                goto unhandled;
            }
            retval = emu->CP0_Cause;
            break;
        
        case 15:
            retval = 0x00018000; //processor id, just copied qemu 4kc
            break;
        case 16:
            if (sel == 0) {
                retval = 0x80008082; // XXX cacheability fields shouldnt be hardcoded as writeable
                break;
            }
            if (sel == 1) {
                retval = 0x1e190c8a;
                break;
            }
            goto unhandled;
        
        case 18:
        case 19:
            retval = 0;
            break;
        
        default:
            unhandled:
            printf("unhandled cp0 reg selector in mfc0 %d %d\n",regNum,sel);
            exit(1);
    }
    
    setRt(emu,op,retval);
}

static void op_mtc0(Mips * emu,uint32_t op) {
    uint32_t rt = getRt(emu,op);
    uint32_t regNum = (op&0xf800) >> 11;
    uint32_t sel = op & 7;
    
    switch(regNum) {
        
        case 0: // Index
            if(sel != 0) {
               goto unhandled; 
            }
            
            emu->CP0_Index = rt & 0xf;
            break;
        case 2: // EntryLo0
            emu->CP0_EntryLo0 = rt & (0x3fffffff);
            break;
            
        case 3: // EntryLo1
            emu->CP0_EntryLo1 = rt & (0x3fffffff);
            break;
        
        case 4: // Context
            emu->CP0_Context = rt & (0xff8);
            break;
         
        case 5: // Page Mask
            if(sel != 0) {
                goto unhandled;
            }
            if (rt) {
                puts("untested page mask!");
                exit(1);
            }
            
            emu->CP0_PageMask = rt & 0x1ffe000;
            break;

        case 6: // Wired
            if(sel != 0) {
                goto unhandled;
            }
            emu->CP0_Wired = rt & 0xf;
            break;

        case 9: // Count
            if(sel != 0) {
                goto unhandled;
            }
            emu->CP0_Count = rt;
            break;


        case 10: // EntryHi
            if (sel != 0 ) {
                goto unhandled;
            }
            emu->CP0_EntryHi = rt & (~0x1f00);
            break;
        
        case 11: // Compare
            if(sel != 0) {
                goto unhandled;
            }
            emu->CP0_Compare = rt;
            break;
       
        case 12: // Status
            if (sel != 0 ) {
                goto unhandled;
            }
            uint32_t status_mask = 0x7d7cff17;
            emu->CP0_Status =  (emu->CP0_Status & ~status_mask ) | (rt & status_mask); //mask out read only
            //XXX NMI is one way write
            break;
        
        case 13:
            break;
        
        case 16:
            break;
        
        case 18:
        case 19:
            break;
                
        default:
            unhandled:
            printf("unhandled cp0 reg selector in mtc0 %d %d\n",regNum,sel);
            exit(1);
    }
    
}

static void op_cache(Mips * emu, uint32_t op) {
    /* noop? */
}

static void op_pref(Mips * emu, uint32_t op) {
    /* noop? */
}

static void op_eret(Mips * emu, uint32_t op) {
    
    if(emu->inDelaySlot){
        return;
    }
    
    emu->llbit = 0;
    
    if(emu->CP0_Status & (1 << 2)) { //if ERL is set
        emu->CP0_Status &= ~(1 << 2); //clear ERL;
        emu->pc = emu->CP0_ErrorEpc;
    } else {
        emu->pc = emu->CP0_Epc;
        emu->CP0_Status &= ~(1 << 1); //clear EXL;
    }
}

static void op_tlbwi(Mips * emu, uint32_t op) {
    
    uint32_t idx = emu->CP0_Index;
    
    TLB_entry * tlbent = &emu->tlb.entries[idx];
    tlbent->VPN = (emu->CP0_EntryHi & 0xfffff000) >> 12;
    tlbent->ASID = emu->CP0_EntryHi & 0xff;
    tlbent->G = (emu->CP0_EntryLo0 | emu->CP0_EntryLo1) & 1;
    tlbent->V0 = (emu->CP0_EntryLo0 & 2) > 0;
    tlbent->V1 = (emu->CP0_EntryLo1 & 2) > 0;
    tlbent->D0 = (emu->CP0_EntryLo0 & 4) > 0;
    tlbent->D1 = (emu->CP0_EntryLo1 & 4) > 0;
    tlbent->C0 = (emu->CP0_EntryLo0  >> 3) & 7;
    tlbent->C1 = (emu->CP0_EntryLo1  >> 3) & 7;
    tlbent->PFN[0] = ((emu->CP0_EntryLo0 >> 6) & 0xfffff) << 12;
    tlbent->PFN[1] = ((emu->CP0_EntryLo1 >> 6) & 0xfffff) << 12;
    
}


static void op_sltu(Mips * emu,uint32_t op) {
    uint32_t rs = getRs(emu,op);
    uint32_t rt = getRt(emu,op);
    if (rs < rt) {
        setRd(emu,op,1);
    } else {
        setRd(emu,op,0);
    }
}




static void op_slt(Mips * emu,uint32_t op) {
    int32_t rs = getRs(emu,op);
    int32_t rt = getRt(emu,op);
    if (rs < rt) {
        setRd(emu,op,1);
    } else {
        setRd(emu,op,0);
    }
}




static void op_nor(Mips * emu,uint32_t op) {
    setRd(emu,op,~(getRs(emu,op) | getRt(emu,op)));
}




static void op_xor(Mips * emu,uint32_t op) {
   setRd(emu,op,getRs(emu,op) ^ getRt(emu,op));
}




static void op_or(Mips * emu,uint32_t op) {
    setRd(emu,op,getRs(emu,op) | getRt(emu,op));
}




static void op_and(Mips * emu,uint32_t op) {
    setRd(emu,op,getRs(emu,op) & getRt(emu,op));
}




static void op_subu(Mips * emu,uint32_t op) {
        setRd(emu,op,getRs(emu,op) -  getRt(emu,op));
}




static void op_sub(Mips * emu,uint32_t op) {
    setRd(emu,op,getRs(emu,op) -  getRt(emu,op));
}




static void op_addu(Mips * emu,uint32_t op) {
    setRd(emu,op,getRs(emu,op) + getRt(emu,op));
}




static void op_add(Mips * emu,uint32_t op) {
    setRd(emu,op,getRs(emu,op) + getRt(emu,op));
}


static void op_ll(Mips * emu,uint32_t op) {
	int16_t offset = getImm(op);
	uint32_t addr = ((int32_t)getRs(emu,op) + offset);
	uint32_t v = readVirtWord(emu,addr);
	if(emu->exceptionOccured) {
        return;
    }
    emu->llbit = 1;
	setRt(emu,op,v);
}

static void op_sc(Mips * emu,uint32_t op) {
	int16_t offset = getImm(op);
	uint32_t addr = ((int32_t)getRs(emu,op) + offset);
	if(emu->llbit) {
	    writeVirtWord(emu,addr,getRt(emu,op));
	}
	setRt(emu,op,emu->llbit);
	
}


static void op_divu(Mips * emu,uint32_t op) {
    uint32_t rs = getRs(emu,op);
    uint32_t rt = getRt(emu,op);
    
    if(rt == 0) {
        return;
    }
    
    emu->lo = rs / rt;
    emu->hi = rs % rt;
    
}




static void op_div(Mips * emu,uint32_t op) {
    int32_t rs = getRs(emu,op);
    int32_t rt = getRt(emu,op);
    
    if(rt == 0) {
        return;
    }
    
    emu->lo = rs / rt;
    emu->hi = rs % rt;
}




static void op_multu(Mips * emu,uint32_t op) {
    uint64_t result = (uint64_t)getRs(emu,op) * (uint64_t)getRt(emu,op);
    emu->hi = result >> 32;
    emu->lo = result & 0xffffffff; 
}




static void op_mult(Mips * emu,uint32_t op) {
    int64_t result = (int64_t)(int32_t)getRs(emu,op) * (int64_t)(int32_t)getRt(emu,op);
    emu->hi = result >> 32;
    emu->lo = result & 0xffffffff;    
}

static void op_movz(Mips * emu,uint32_t op) {
    if(getRt(emu,op) == 0) {
        setRd(emu,op,getRs(emu,op));
    }
}

static void op_movn(Mips * emu,uint32_t op) {
    if(getRt(emu,op) != 0) {
        setRd(emu,op,getRs(emu,op));
    }
}


static void op_mul(Mips * emu,uint32_t op) {
    int32_t result = (int32_t)getRs(emu,op) * (int32_t)getRt(emu,op);
    setRd(emu,op,result);
}


static void op_mflo(Mips * emu,uint32_t op) {
    setRd(emu,op,emu->lo);
}




static void op_mfhi(Mips * emu,uint32_t op) {
    setRd(emu,op,emu->hi);
}




static void op_jalr(Mips * emu,uint32_t op) {
	emu->delaypc = getRs(emu,op);
	emu->regs[31] = emu->pc + 8;
	emu->inDelaySlot = 1;
}




static void op_jr(Mips * emu,uint32_t op) {
    emu->delaypc = getRs(emu,op);
	emu->inDelaySlot = 1;
}




static void op_srav(Mips * emu,uint32_t op) {
    setRd(emu,op,((int32_t)getRt(emu,op) >> (getRs(emu,op)&0x1f)));
}




static void op_srlv(Mips * emu,uint32_t op) {
    setRd(emu,op,(getRt(emu,op) >> (getRs(emu,op)&0x1f)));
}




static void op_sllv(Mips * emu,uint32_t op) {
    setRd(emu,op,(getRt(emu,op) << (getRs(emu,op)&0x1f)));
}




static void op_sra(Mips * emu,uint32_t op) {
	int32_t rt = getRt(emu,op);
	int32_t sa = getShamt(op);
	int32_t v = rt >> sa;
	setRd(emu,op,v);
}




static void op_srl(Mips * emu,uint32_t op) {
	uint32_t v = getRt(emu,op) >> getShamt(op);
	setRd(emu,op,v);
}




static void op_sll(Mips * emu,uint32_t op) {
    uint32_t v = getRt(emu,op) << getShamt(op);
	setRd(emu,op,v);
}




static void op_bgez(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) >= 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
	} else {
		emu->delaypc = emu->pc + 8;
	}
	emu->inDelaySlot = 1;
}

static void op_bgezl(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) >= 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
		emu->inDelaySlot = 1;
	} else {
		emu->pc += 4;
	}
}


static void op_bltz(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) < 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
	} else {
		emu->delaypc = emu->pc + 8;
	}
	emu->inDelaySlot = 1;
}

static void op_bltzl(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) < 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
		emu->inDelaySlot = 1;
	} else {
		emu->pc += 4;
	}
}



#include "./gen/doop.gen.c"


