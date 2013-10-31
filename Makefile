.PHONY: all clean

all: emu

emu: ./src/*.c ./src/gen/doop.gen.c ./include/*.h 
	gcc -O2 -I./include/ ./src/*.c -lpthread  -o emu


./src/gen/doop.gen.c: ./disgen/*.py ./disgen/mips.json
	mkdir -p ./src/gen/
	python ./disgen/disgen.py ./disgen/cdisgen.py ./disgen/mips.json > ./src/gen/doop.gen.c

clean:
	rm -vf ./src/common/gen/
	rm -fv ./emu
