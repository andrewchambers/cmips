all: emu ememu.js

emu: ./src/*.c ./include/*.h ./src/gen/doop.c
	gcc -O2 -I./include/ ./src/*.c -o emu

ememu.js: ./src/*.c ./include/*.h ./src/gen/doop.c
	emcc -s TOTAL_MEMORY=134217728 -O2 -I./include/ ./src/*.c -o ememu.js

./src/gen/doop.c: ./disgen/*.py ./disgen/mips.json
	mkdir -p ./src/gen
	python ./disgen/disgen.py ./disgen/cdisgen.py ./disgen/mips.json > ./src/gen/doop.c

clean:
	rm -vrf ./src/gen/
	rm -fv ./emu
