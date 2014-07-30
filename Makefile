
EXE = subleq-asm

all: $(EXE)
	g++ *.cpp -o $(EXE) -std=c++11

test: all
	tests/test.sh

