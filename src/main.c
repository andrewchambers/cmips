#include "mips.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>



pthread_mutex_t emu_mutex;


void * runEmulator(void * p) {
    Mips * emu = (Mips *)p;
    
    while(emu->shutdown != 1) {
        int i;
        
        if(pthread_mutex_lock(&emu_mutex)) {
            puts("mutex failed lock, exiting");
            exit(1);
        }

        for(i = 0; i < 1000 ; i++) {
            step_mips(emu);
        }
        
        if(pthread_mutex_unlock(&emu_mutex)) {
            puts("mutex failed unlock, exiting");
            exit(1);
        }
        
    }
    free_mips(emu);
    exit(0);
    
}




int main(int argc,char * argv[]) {
        
    pthread_t emu_thread;
    
    if(pthread_mutex_init(&emu_mutex,NULL)) {
        puts("failed to create mutex");
        return 1;
    }
    
    if (argc < 2) {
        printf("Usage: %s image.srec\n",argv[0]);
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
    
    
    if(pthread_create(&emu_thread,NULL,runEmulator,emu)) {
        puts("creating emulator thread failed!");
        return 1;
    }
    
    while(1) {
        int c = getchar();
        if(c == EOF) {
            exit(1);
        }
        
        if(pthread_mutex_lock(&emu_mutex)) {
            puts("mutex failed lock, exiting");
            exit(1);
        }
        uart_RecieveChar(emu,c);
        if(pthread_mutex_unlock(&emu_mutex)) {
            puts("mutex failed unlock, exiting");
            exit(1);
        }
    }
    
    
    return 0;
}
