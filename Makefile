CC 		:= clang++
CFLAGS 	:= -Wall -Wextra -pedantic 

.PHONY: read-macho write-macho all clean

all: read-macho write-macho

clean: 
	-$(RM) read-macho.o write-macho.o read-macho write-macho

read-macho: read-macho.o
	$(CC) -o $@ $< 

write-macho: write-macho.o
	$(CC) -o $@ $<

%.o: %.cpp
	$(CC) -std=c++11 $(CFLAGS) -o $@ $< -c
