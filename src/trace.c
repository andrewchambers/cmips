#include "mips.h"

#include <stdlib.h>
#include <stdio.h>

EmuTrace * startTrace(char * tracefile) {
    EmuTrace * ret = calloc(sizeof(EmuTrace),1);
    if(!ret) {
        return 0;
    }
    FILE * f = fopen(tracefile,"w");
    
    if(!f) {
        free(ret);
        return 0;
    }
    
    ret->file = f;
    return ret;
}

void updateTrace(EmuTrace * t,Mips * emu) {
    int needsComma = 0;
    int i;
    fprintf(t->file,"{");
    for(i = 0; i < 32; i++){
        if (t->regs[i] != emu->regs[i]) {
            if (needsComma) {
                fprintf(t->file,",");
                needsComma = 0;
            }   
        
            fprintf(t->file," \"%s\" : %u ",regn2o32[i],emu->regs[i]);
            t->regs[i] = emu->regs[i];
            needsComma = 1;
        }
    }
    
    
    if (t->pc != emu->pc) {
        if (needsComma) {
            fprintf(t->file,",");
            needsComma = 0;
        }  
        needsComma = 1;
        fprintf(t->file," \"%s\" : %u ","pc",emu->pc);
        t->pc = emu->pc;
    }
    
    if (t->hi != emu->hi) {
        if (needsComma) {
            fprintf(t->file,",");
            needsComma = 0;
        }  
        fprintf(t->file," \"%s\" : %u ","hi",emu->hi);
        t->hi = emu->hi;
        needsComma = 1;
    } 
    
    if (t->lo != emu->lo) {
        if (needsComma) {
            fprintf(t->file,",");
            needsComma = 0;
        }
        fprintf(t->file," \"%s\" : %u ","lo",emu->lo);
        t->lo = emu->lo;
    }
    
    fprintf(t->file,"}\n");   
    //flush output so errors etc dont ruin log.
    fflush(t->file);
    
}

void endTrace(EmuTrace * t) {
    fclose(t->file);
    free(t);
}
