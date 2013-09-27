
#include "mips.h"
#include <stdlib.h>
#include <stdio.h>




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


//Accessing a phys address which doesnt exist
#define BUS_ERROR 1
#define UNALIGNED_ERROR 2

//returns an error code
static inline int translateAddress(Mips * emu,uint32_t addr,uint32_t * paddr_out) {
    
    uint32_t paddr;
    
    
    if ( addr >= 0x80000000 && addr <= 0x9fffffff ) {
        paddr = addr - 0x80000000;
    } else if ( addr >= 0xa0000000 && addr <= 0xbfffffff ) {
        paddr = addr - 0xa0000000;
    } else {
        *paddr_out = addr;
        return BUS_ERROR;
    }
    
    
    *paddr_out = paddr;
    return 0;
}


static uint32_t readVirtWord(Mips * emu, uint32_t addr) {
    uint32_t paddr;
    int err = translateAddress(emu,addr,&paddr);
    if(err) {
        //printf("Unhandled Memory error %d reading addr %08x\n",err,addr);
        //exit(1);
        
    }
    
    if(paddr % 4 != 0) {
        printf("Unhandled alignment error reading addr %08x\n",addr);
        exit(1); 
    }
    
    if(paddr >= UARTBASE && paddr <= UARTBASE + UARTSIZE) {
        return uart_read(emu,paddr - UARTBASE);
    }
    
    if (paddr >= emu->pmemsz) {
        puts("unhandled bus error");
        exit(1);
    }
    
    return emu->mem[paddr/4];    
}

static void writeVirtWord(Mips * emu, uint32_t addr,uint32_t val) {
    uint32_t paddr;
    int err = translateAddress(emu,addr,&paddr);
    if(err) {
        //printf("Unhandled Memory error %d writing %08x to %08x\n",err,val,addr);
        //exit(1);
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
        puts("unhandled bus error");
        exit(1);
    }
    
    emu->mem[paddr/4] = val;    
}

static uint8_t readVirtByte(Mips * emu, uint32_t addr) {
    uint32_t paddr;
    int err = translateAddress(emu,addr,&paddr);
    if(err) {
        //printf("Unhandled Memory error %d reading byte at addr %08x\n",err,addr);
        //exit(1);
    }
    
    if(paddr >= UARTBASE && paddr <= UARTBASE + UARTSIZE) {
        return uart_readb(emu,paddr - UARTBASE);
    }
    
    unsigned int offset = paddr&3;
    
    if (paddr >= emu->pmemsz) {
        puts("unhandled bus error");
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
    
    int err = translateAddress(emu,addr,&paddr);
    if(err) {
        //printf("Unhandled Memory error %d writing %02x to %08x\n",err,val,addr);
        //exit(1);
    }
    
    if(paddr >= UARTBASE && paddr <= UARTBASE + UARTSIZE) {
        uart_writeb(emu,paddr - UARTBASE,val);
        return;
    }
    
    if(paddr >= POWERBASE && paddr <= POWERBASE + POWERSIZE) {
        exit(0);
    }
    
    if (paddr >= emu->pmemsz) {
        puts("unhandled bus error");
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


void step_mips(Mips * emu) {

	int startInDelaySlot = emu->inDelaySlot;
	
	uint32_t opcode = readVirtWord(emu,emu->pc);
	
    doop(emu,opcode);
    emu->regs[0] = 0;

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




static void op_lbu(Mips * emu,uint32_t op) {
	uint32_t addr = ((int32_t)getRs(emu,op) + (int16_t)getImm(op));
	uint32_t v = readVirtByte(emu,addr);
	setRt(emu,op,v);
}




static void op_lw(Mips * emu,uint32_t op) {
	int16_t offset = getImm(op);
	uint32_t addr = ((int32_t)getRs(emu,op) + offset);
	uint32_t v = readVirtWord(emu,addr);
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
	writeVirtByte(emu,addr+1,vlo);
}




static void op_slti(Mips * emu,uint32_t op) {
    int32_t rs = getRs(emu,op);
    int16_t c = getImm(op);
    if ( rs < c ) { 
        setRt(emu,op,1);
    } else {
        setRt(emu,op,0);
    }
}




static void op_sltiu(Mips * emu,uint32_t op) {
    uint32_t rs = getRs(emu,op);
    uint32_t c = (uint32_t)(int16_t)getImm(op);
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
	uint8_t vhi = readVirtByte(emu,addr);
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
	uint8_t vhi = readVirtByte(emu,addr);
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




static void op_blez(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) <= 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
	} else {
		emu->delaypc = emu->pc + 8;
	}
	emu->inDelaySlot = 1;
}




static void op_lwr(Mips * emu,uint32_t op) {
    int32_t c = (int16_t)(op&0x0000ffff);
    uint32_t addr = ((int32_t)getRs(emu,op))+c;
    uint32_t rtVal = getRt(emu,op);
    uint32_t wordVal = readVirtWord(emu,addr & (~3));
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
    printf("unimplemented opcode op_mfc0 %08x at pc %08x\n",op,emu->pc);
    exit(1);
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
    printf("unimplemented opcode op_sub %08x at pc %08x\n",op,emu->pc);
    exit(1);
}




static void op_addu(Mips * emu,uint32_t op) {
    setRd(emu,op,getRs(emu,op) + getRt(emu,op));
}




static void op_add(Mips * emu,uint32_t op) {
    setRd(emu,op,getRs(emu,op) + getRt(emu,op));
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
    int64_t result = (int64_t)getRs(emu,op) * (int64_t)getRt(emu,op);
    emu->hi = result >> 32;
    emu->lo = result & 0xffffffff;    
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




static void op_bltz(Mips * emu,uint32_t op) {
	int32_t offset = sext18(getImm(op) * 4);
	if (((int32_t)getRs(emu,op)) < 0) {
		emu->delaypc = (int32_t)(emu->pc + 4) + offset;
	} else {
		emu->delaypc = emu->pc + 8;
	}
	emu->inDelaySlot = 1;
}



#include "gen/doop.c"


