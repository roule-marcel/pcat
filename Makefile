SOURCES:=$(shell find src -name "*.c")
OBJECTS:=$(SOURCES:src/%.c=bin/%.o)

all: pcat

pcat: $(OBJECTS)
	gcc -g -o $@ $^ -pthread

	
bin/%.o: src/%.c
	mkdir -p `dirname $@`
	gcc -g -o $@ -c $<
	
	
clean:
	rm -rf bin

install:
	cp pcat /usr/bin	