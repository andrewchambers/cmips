#include "mips.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>



pthread_mutex_t emu_mutex;




int ttyraw()
{
	int fd = STDIN_FILENO;
	struct termios oldtermios;
	/* Set terminal mode as follows:
	   Noncanonical mode - turn off ICANON.
	   Turn off signal-generation (ISIG)
	   	including BREAK character (BRKINT).
	   Turn off any possible preprocessing of input (IEXTEN).
	   Turn ECHO mode off.
	   Disable CR-to-NL mapping on input.
	   Disable input parity detection (INPCK).
	   Disable stripping of eighth bit on input (ISTRIP).
	   Disable flow control (IXON).
	   Use eight bit characters (CS8).
	   Disable parity checking (PARENB).
	   Disable any implementation-dependent output processing (OPOST).
	   One byte at a time input (MIN=1, TIME=0).
	*/
	struct termios newtermios;
	if(tcgetattr(fd, &oldtermios) < 0)
		return 1;
	newtermios = oldtermios;

	newtermios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	/* OK, why IEXTEN? If IEXTEN is on, the DISCARD character
	   is recognized and is not passed to the process. This 
	   character causes output to be suspended until another
	   DISCARD is received. The DSUSP character for job control,
	   the LNEXT character that removes any special meaning of
	   the following character, the REPRINT character, and some
	   others are also in this category.
	*/

	newtermios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	/* If an input character arrives with the wrong parity, then INPCK
	   is checked. If this flag is set, then IGNPAR is checked
	   to see if input bytes with parity errors should be ignored.
	   If it shouldn't be ignored, then PARMRK determines what
	   character sequence the process will actually see.
	   
	   When we turn off IXON, the start and stop characters can be read.
	*/

	newtermios.c_cflag &= ~(CSIZE | PARENB);
	/* CSIZE is a mask that determines the number of bits per byte.
	   PARENB enables parity checking on input and parity generation
	   on output.
	*/

	newtermios.c_cflag |= CS8;
	/* Set 8 bits per character. */

	newtermios.c_oflag &= ~(OPOST);
	/* This includes things like expanding tabs to spaces. */

	newtermios.c_cc[VMIN] = 1;
	newtermios.c_cc[VTIME] = 0;

	/* You tell me why TCSAFLUSH. */
	if(tcsetattr(fd, TCSAFLUSH, &newtermios) < 0)
		return -1;
	return 0;
}

static void printstate(Mips * emu,uint64_t n) {
    int i;
    fprintf(stderr,"State: %d\n",n);
    for(i = 0; i < 32; i++){
        fprintf(stderr,"%s%d: %08x\n","gr",i,emu->regs[i]);
    }
    
    #define PRFIELD(X) fprintf(stderr, #X ": %08x\n",emu->X); 
    PRFIELD(hi);
    PRFIELD(lo);
    PRFIELD(pc);
    PRFIELD(delaypc);
    PRFIELD(CP0_Index);
    PRFIELD(CP0_EntryHi);
    PRFIELD(CP0_EntryLo0);
    PRFIELD(CP0_EntryLo1);
    PRFIELD(CP0_Context);
    PRFIELD(CP0_Wired);
    PRFIELD(CP0_Status);
    PRFIELD(CP0_Epc);
    PRFIELD(CP0_BadVAddr);
    PRFIELD(CP0_ErrorEpc);
    PRFIELD(CP0_Cause);
    PRFIELD(CP0_PageMask);
    PRFIELD(CP0_Count);
    PRFIELD(CP0_Compare);
    #undef PRFIELD
    
}

void * runEmulator(void * p) {
    Mips * emu = (Mips *)p;
    
    uint64_t n = 0;

    while(emu->shutdown != 1) {
        int i;
        
        if(pthread_mutex_lock(&emu_mutex)) {
            puts("mutex failed lock, exiting");
            exit(1);
        }

        for(i = 0; i < 1000 ; i++) {
            if(n > 441320000) {
                printstate(emu,n);
            }
            step_mips(emu);
            n++;
            if(n >= 450000000) {
                exit(0);
            }
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
    
	//if(ttyraw()) {
	//	puts("failed to configure raw mode");
	//	return 1;
	//}
    
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
