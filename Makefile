CC  = gcc
RM = rm

CFLAGS  = -O -c -pthread -g -Wall -o $@

snc: main.o
	$(CC) -o snc -lpthread main.o

main.o: main.c
	$(CC) $(CFLAGS) $<

clean:
	-$(RM) *.o snc



