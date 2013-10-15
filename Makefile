.PHONY: all clean jsemu

all: emu

emu: ./src/common/*.c ./src/desktop/*.c ./src/common/gen/doop.gen.c ./include/*.h 
	gcc -O0 -g -I./include/ ./src/common/*.c ./src/desktop/*.c -o emu

#currently disabled
jsemu: ./src/web/ememu.js

./src/web/ememu.js: ./src/common/*.c ./include/*.h ./src/common/gen/doop.gen.c
	emcc -s EXPORTED_FUNCTIONS="['_new_mips','_free_mips','_step_mips','_loadSrecFromString_mips']" -s TOTAL_MEMORY=134217728 -O2 -I./include/ ./src/common/*.c -o ./src/web/ememu.js

./src/common/gen/doop.gen.c: ./disgen/*.py ./disgen/mips.json
	mkdir -p ./src/common/gen/
	python ./disgen/disgen.py ./disgen/cdisgen.py ./disgen/mips.json > ./src/common/gen/doop.gen.c

clean:
	rm -vf ./src/web/ememu.js
	rm -vf ./src/common/gen/doop.gen.c
	rm -fv ./emu
