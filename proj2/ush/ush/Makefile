CC = gcc
OPT = -std=gnu99
CFLAGS = $(OPT)
SRC = parse.c ush.c
EXE = ush 
HEADER = parse.h ush.h

all: $(*.c.o)
	$(CC) -o $(EXE) $(CFLAGS) $(SRC)

debug: $(*.c.o)
	$(CC) -DDEBUG_MODE -g -O0 -o $(EXE) $(CFLAGS) $(SRC)

.c.o: $(HEADER)
	$(CC) $(CFLAGS) -c $(*.c)

clean: 
	rm -f ush *.o
