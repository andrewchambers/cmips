all: emu

emu: ./src/*.c ./include/*.h ./src/gen/doop.c
	gcc -g -I./include/ ./src/*.c -o emu

./src/gen/doop.c: ./disgen/*.py ./disgen/mips.json
	mkdir -p ./src/gen
	python ./disgen/disgen.py ./disgen/cdisgen.py ./disgen/mips.json > ./src/gen/doop.c

clean:
	rm -vrf ./src/gen/
	rm -fv ./emu
