CC = gcc
CFLAGS = -g -Wall #-W -Wextra
LFLAGS = -lz #zlib.h => CRC
PROG_SENDER = sender
PROG_RECEIVER = receiver
SRC = src

all : $(PROG_SENDER) $(PROG_RECEIVER)

$(PROG_SENDER) : sender.o
	$(CC) $(CFLAGS) -o $(PROG_SENDER) sender.o $(LFLAGS)

sender.o : $(SRC)/sender.c $(SRC)/defines.h
	$(CC) $(CFLAGS) -c $(SRC)/sender.c $(LFLAGS)

$(PROG_RECEIVER) : receiver.o
	$(CC) $(CFLAGS) -o $(PROG_RECEIVER) receiver.o $(LFLAGS)

receiver.o : $(SRC)/receiver.c $(SRC)/defines.h
	$(CC) $(CFLAGS) -c $(SRC)/receiver.c $(LFLAGS)

clean : #-f prevents an error message to be displayed if no file were found to be deleted
	rm -f *.o

cleanall : clean
	rm -f $(PROG_SENDER)
	rm -f $(PROG_RECEIVER)
