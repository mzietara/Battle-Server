CC = gcc
PORT=31313
CFLAGS = -DPORT=\$(PORT) -g -Wall

battle: battle.o
	${CC} ${CFLAGS} -o battle battle.o

battle.o: battle.c battle.h
	${CC} ${CFLAGS} -c battle.c

clean:
	rm *.o battle
