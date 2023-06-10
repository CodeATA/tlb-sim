SRC := $(wildcard *.cpp)

all: tlb-sim

tlb-sim: $(SRC)
	g++ -o $@ -g -Wno-unused-result -O2 $(SRC)

.PHONY: clean
clean:
	rm -f tlb-sim