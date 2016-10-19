CC = gcc
CFLAGS = -g -Wall #-W -Wextra
LFLAGS = -lz #zlib.h => CRC
PROG_SENDER = sender
PROG_RECEIVER = receiver
SRC = src

all : $(PROG_SENDER) $(PROG_RECEIVER)

$(PROG_SENDER) : sender.o createConnection.o
	$(CC) $(CFLAGS) -o $(PROG_SENDER) sender.o createConnection.o $(LFLAGS)

sender.o : $(SRC)/sender.c $(SRC)/defines.h
	$(CC) $(CFLAGS) -c $(SRC)/sender.c $(LFLAGS)

$(PROG_RECEIVER) : receiver.o createConnection.o
	$(CC) $(CFLAGS) -o $(PROG_RECEIVER) receiver.o createConnection.o $(LFLAGS)

receiver.o : $(SRC)/receiver.c $(SRC)/defines.h
	$(CC) $(CFLAGS) -c $(SRC)/receiver.c $(LFLAGS)

createConnection.o : $(SRC)/createConnection.c
	$(CC) $(CFLAGS) -c $(SRC)/createConnection.c

readWriteLoop.o : $(SRC)/readWriteLoop.c
	$(CC) $(CFLAGS) -c $(SRC)/readWriteLoop.c

clean :
	@rm -f *.o #-f prevents an error message to be displayed if no file were found to be deleted

cleanall : clean
	@rm -f $(PROG_SENDER)
	@rm -f $(PROG_RECEIVER)
