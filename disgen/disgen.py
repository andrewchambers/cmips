#Script to convert the json instruction description into a switch case.
#Uses a simple plugin system (exec based) because I reused this across a few projects.


import sys
import json
import itertools

#replace anything that is not binary with x
#remove spaces
def fixOpstring(mstring):
    ret = ""
    for c in mstring:
        if c in "01":
            ret += c
        elif c == " ":
            continue
        else:
            ret += "x"
    return ret

#test for opstring equivalence
#used to make sure all different
#values can really be distinguished
def canNotDistinguishOpstring(m1,m2):
    assert len(m1) == len(m2)
    
    for idx in range(len(m1)):
        c1,c2 = m1[idx],m2[idx]
        if c1 in "01" and c2 not in "01":
            continue
        if c2 in "01" and c1 not in "01":
            continue
        
        if c1 != c2:
            return False
    
    return True

def ensureValid(input):
    names = map(lambda x : x[0],input)
    if len(set(names)) != len(names):
        raise Exception("non unique names in input")
    
    opstrings = map(lambda x : x[1],input)
    
    for s in opstrings:
        if len(s) != 32:
            raise Exception("all opstrings must be 32 bits - " + s)
    

#input is same as informat
def ensureCanDistinguish(input):
    opstrings = map(lambda x : x[1],input)
    for a,b in itertools.combinations(opstrings,2):
        if canNotDistinguishOpstring(a,b):
            raise Exception("opstrings not distinguishable! %s %s" %(a,b))

def opstringToMask(opstring):
    andMask = ""
    for c in opstring:
        if c in "01":
            andMask += "1"
        else:
            andMask += "0"
    return andMask

class Counter(object):
    def __init__(self,col):
        self.counts = {}
        for x in col:
            if x in self.counts:
                self.counts[x] += 1
            else:
                self.counts[x] = 0
    def __iter__(self):
        return iter(self.counts)
    def __getitem__(self,idx):
        return self.counts[idx]

def findBestAndMask(input):
    opstrings = map(lambda x : x[1],input)
    andMasks = map(opstringToMask,opstrings)
    counts = Counter(andMasks)
    andMask = max(counts,key=lambda x : counts[x])
    return andMask
    
    
def opstringToVal(opstring):
    val = 0
    for idx,c in enumerate(opstring):
        if c == "1":
            val += 2**(len(opstring) - 1 -idx)
    return val

    
class CodeGenerator(object):
    
    def __init__(self):
        self.depth = 0
    
    @property
    def ws(self):
        return "    " * self.depth
    
    def generate(self,masks):
        self.depth = 0
        
        self.startFunc()
        self.depth += 1
        for m in masks:
            self.startSwitch(m[0])
            self.depth += 1
            for value,name in m[1]:
                self.genCase(name,value)
            self.depth -= 1
            self.endSwitch()
        self.depth -= 1
        self.endFunc()

def getCodeGenerator(fname):
    locals = {}
    execfile(fname,{"CodeGenerator":CodeGenerator},locals)
    for v in locals:
        if issubclass(locals[v],CodeGenerator):
            return locals[v]()
    raise Exception("No CodeGenerator subclass found")
    
    

def main():
    data = open(sys.argv[2]).read()
    input = json.loads(data)
    input.sort(key=lambda x:x[1])
    for k in range(len(input)):
        input[k][1] = fixOpstring(input[k][1])
    ensureValid(input)
    ensureCanDistinguish(input)
    
    #list of  [andmask , [[val,name] ...] ...]
    #mask is what you and the instruction with
    #val is the result
    #eventually to decode you will move down the list
    #in order testing masks and switching on val
    masks = []
    
    #we iterate until we shift all from input into the masks array
    while len(input):
        newinput = []
        bestAndmask = findBestAndMask(input)
        masks.append([bestAndmask , []])
        
        for name,mask in input:
            if opstringToMask(mask) == bestAndmask:
                masks[-1][1].append([opstringToVal(mask),name])
            else:
                newinput.append([name,mask])
                
        input = newinput
    
    
    g = getCodeGenerator(sys.argv[1])
    g.generate(masks)
    
if __name__ == "__main__":
    main()
