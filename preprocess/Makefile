SRC := $(wildcard *.cpp)

all: preprocess preprocess-check

preprocess: $(SRC)
	g++ -o $@ -g -Wno-unused-result -O2 $(SRC)

preprocess-check: $(SRC)
	g++ -o $@ -g -Wno-unused-result -DONLY_CHECK -O2 $(SRC)

.PHONY: clean
clean:
	rm -f preprocess