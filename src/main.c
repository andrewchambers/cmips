#include "mips.h"
#include <stdio.h>


int main(int argc,char * argv[]) {
        
    if (argc < 2) {
        printf("Usage: %s image.srec [tracefile]\n",argv[0]);
        return 1;
    }
    
    Mips * emu = new_mips(64 * 1024 * 1024);
    
    if (!emu) {
        puts("allocating emu failed.");
        return 1;
    }
    
    if (loadSrecFromFile_mips(emu,argv[1]) != 0) {
        puts("failed loading srec");
        return 1;
    }
    
    EmuTrace * tr = 0;
    
    if(argc > 2) {
        tr = startTrace(argv[2]);
        if(!tr) {
            printf("failed to start trace %s.\n",argv[2]);
        }
    }
    
    while(emu->shutdown != 1) {
        if(tr) {
            updateTrace(tr,emu);
        }
        step_mips(emu);
    }
    
    if(tr) {
        endTrace(tr);
    }
    free_mips(emu);
    
    return 0;
}
