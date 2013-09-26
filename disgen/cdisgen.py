

class CGen(CodeGenerator):
    
    def startFunc(self):
        print self.ws + "void doop(Mips * emu, uint32_t op) {"
    
    def endFunc(self):
        print self.ws + "    printf(\"unhandled opcode at %x -> %x\\n\",emu->pc,op);"
        print self.ws + "    exit(1);"
        print self.ws + "}"
    
    def startSwitch(self,switch):
        print self.ws + "switch(op & %s) {" % hex(int(switch,2))
        
    def genCase(self,name,value):
        self.depth -= 1
        print self.ws + "    case %s:"%hex(value)
        print self.ws + "        op_%s(emu,op);"%name
        print self.ws + "        return;"
        self.depth += 1
        
    def endSwitch(self):
        print self.ws + "}"
