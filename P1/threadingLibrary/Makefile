OPT = -std=gnu99
CFLAGS = $(OPT)

mythread.a: mythread.o 
	ar rcs mythread.a mythread.o

mythread.o: mythread.c
	gcc $(CFLAGS) -c mythread.c

clean:
	rm -f *.o mythread.a
