#include "mips.h"

#include <stdio.h>
#include <stdlib.h>


static int fnextChar(void * ud) {
    int c = fgetc((FILE*)ud);
    if(c == EOF) {
        return -1;
    }
    return c;
}

static int fisEof(void * ud) {
    return feof((FILE*)ud);
}


int loadSrecFromFile_mips(Mips * emu,char * fname) {
    
    void * ud = fopen(fname,"r");
    
    if(!ud) {
        fprintf(stdout,"srec %s failed to open",fname);
        return 1;
    }
    
    SrecLoader loader;
    loader.nextChar = &fnextChar;
    loader.isEof = &fisEof;
    loader.userdata = ud;
    int ret = loadSrec_mips(emu,&loader);
    fclose((FILE*)ud);
    return ret;
}


static int snextChar(void * ud) {
    int c = **(char**)ud;
    (*(char**)ud)++;
    if(c == 0) {
        return -1;
    }
    return c;
}

static int sisEof(void * ud) {
    return (**(char**)ud) == 0;
}

int loadSrecFromString_mips(Mips * emu,char * srec) {
    
    void * ud = (void*)&srec;
    
    SrecLoader loader;
    loader.nextChar = &snextChar;
    loader.isEof = &sisEof;
    loader.userdata = ud;
    int ret = loadSrec_mips(emu,&loader);
    return ret;
}


//a minimized version of address translation which checks fixed map ranges
static void writeb(Mips * emu,uint32_t addr,uint8_t v) {
    //printf("loading %02x to %08x\n",v,addr);
    
    if ( addr >= 0x80000000 && addr <= 0x9fffffff ) {
        addr -= 0x80000000;
    } else if ( addr >= 0xa0000000 && addr <= 0xbfffffff ) {
        addr -= 0xa0000000;
    } else {
        return;
    }
    
    if (addr > emu->pmemsz) {
        return;
    }
    
    uint32_t word = emu->mem[addr / 4];
    unsigned int offset = addr % 4;
    int shamt = 8*(3 - offset);
    word = (word & ~(0xff << shamt)) | (v << shamt);
    emu->mem[addr / 4] = word;
    
}

static int isHexChar(char c){
    if(c >= '0' && c <= '9')
        return 1;
    if(c >= 'a' && c <= 'f')
        return 1;
    if(c >= 'A' && c <= 'F')
        return 1;
    return 0;
}

static int srecReadType(SrecLoader * loader){
    int c = loader->nextChar(loader->userdata);
    if(c == -1){
        return -1; // 0 type srecord will trigger a skip, which will terminate
    }
    if(c != 'S'){
        return -2;
    }
    c = loader->nextChar(loader->userdata) - '0';
    if(c >= 0 && c <= 9)
        return c;
    return -2;
}

static int srecReadByte(SrecLoader * loader, uint8_t * count){
    char chars[3];
    int i;
    
    for(i = 0 ; i < 2; i++){
        chars[i] = loader->nextChar(loader->userdata);
        if(!isHexChar(chars[i]))
            return 1;
    }
    chars[2] = 0;
    *count = (uint8_t) strtoul(&chars[0],0,16);
    return 0;
}

static int srecReadAddress(SrecLoader * loader, uint32_t * addr){
    char chars[9];
    int i;
    
    for(i = 0 ; i < 8; i++){
        chars[i] = loader->nextChar(loader->userdata);
        if(!isHexChar(chars[i]))
            return 1;
    }
    chars[8] = 0;
    
    *addr = strtoul(&chars[0],0,16);
    return 0;
}

static void srecSkipToNextLine(SrecLoader * loader){
    while(1){
        int c = loader->nextChar(loader->userdata);
        if(c == -1 || c == '\n'){
            break;
        }
    }
}

static int srecLoadData(SrecLoader * loader,Mips * emu,uint32_t addr,uint32_t count){
    
    uint8_t b;
    while(count--){
        if(srecReadByte(loader,&b)){
            return 1;
        }
        
        writeb(emu,addr++,b); 
    }
    return 0;
}

int loadSrec_mips(Mips * emu, SrecLoader * loader)
{

    uint32_t addr;
    uint8_t count;
    
    while(!loader->isEof(loader->userdata)){
        switch(srecReadType(loader)){
            
            case -1:
                //EOF
                break;
            case 0:
                srecSkipToNextLine(loader);
                break;
            case 3:
                if(srecReadByte(loader,&count)){
                    fputs("srecLoader: failed to parse bytecount.\n",stdout);
                    return 1;
                }
                if(srecReadAddress(loader,&addr)){
                    fputs("srecLoader: failed to parse address.\n",stdout);
                    return 1;
                }
                if(srecLoadData(loader,emu,addr,count-5)){
                    fputs("srecLoader: failed to load data.\n",stdout);
                    return 1;
                }
                srecSkipToNextLine(loader);
                break;
            case 7:
                if(srecReadByte(loader,&count)){
                    fputs("srecLoader: failed to parse bytecount.\n",stdout);
                    return 1;
                }
                if(srecReadAddress(loader,&addr)){
                    fputs("srecLoader: failed to parse address.\n",stdout);
                    return 1;
                }
                emu->pc = addr;
                srecSkipToNextLine(loader);
                break;

            default:
                fputs("Bad/Unsupported srec type\n",stdout);
                return 1;
        }
    }
    
    return 0;
}
