PROJECT	:= read-macho
CC 		:= clang++
CFLAGS 	:= -Wall -Wextra -pedantic -g

.PHONY: $(PROJECT) all clean realclean

all: $(PROJECT)

clean: 
	-$(RM) $(PROJECT).o

realclean: clean
	-$(RM) $(PROJECT)

$(PROJECT): $(PROJECT).o
	$(CC) -o $@ $< 

%.o: %.cpp
	$(CC) -std=c++11 $(CFLAGS) -o $@ $< -c
