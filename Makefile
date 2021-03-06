CC = gcc
CFLAGS = -g -Wall #-W -Wextra
LFLAGS = -lz #zlib.h => CRC
PROG_SENDER = sender
PROG_RECEIVER = receiver
SRC = src

.PHONY : all clean cleanall tests

all : $(PROG_SENDER) $(PROG_RECEIVER)

$(PROG_SENDER) : sender.o createConnection.o senderReadWriteLoop.o senderManagePackets.o packets.o timer.o
	$(CC) $(CFLAGS) -o $(PROG_SENDER) sender.o createConnection.o senderReadWriteLoop.o senderManagePackets.o packets.o timer.o $(LFLAGS)

sender.o : $(SRC)/sender.c $(SRC)/defines.h createConnection.o senderReadWriteLoop.o
	$(CC) $(CFLAGS) -c $(SRC)/sender.c $(LFLAGS)

$(PROG_RECEIVER) : receiver.o createConnection.o receiverReadWriteLoop.o receiverManagePackets.o packets.o timer.o
	$(CC) $(CFLAGS) -o $(PROG_RECEIVER) receiver.o createConnection.o receiverReadWriteLoop.o receiverManagePackets.o packets.o timer.o $(LFLAGS)

receiver.o : $(SRC)/receiver.c $(SRC)/defines.h createConnection.o receiverReadWriteLoop.o
	$(CC) $(CFLAGS) -c $(SRC)/receiver.c $(LFLAGS)

createConnection.o : $(SRC)/createConnection.c $(SRC)/defines.h
	$(CC) $(CFLAGS) -c $(SRC)/createConnection.c

senderReadWriteLoop.o : $(SRC)/senderReadWriteLoop.c $(SRC)/defines.h senderManagePackets.o timer.o
	$(CC) $(CFLAGS) -c $(SRC)/senderReadWriteLoop.c

receiverReadWriteLoop.o : $(SRC)/receiverReadWriteLoop.c $(SRC)/defines.h receiverManagePackets.o
	$(CC) $(CFLAGS) -c $(SRC)/receiverReadWriteLoop.c

packets.o : $(SRC)/packets.c $(SRC)/packets.h $(SRC)/defines.h
	$(CC) $(CFLAGS) -c $(SRC)/packets.c $(LFLAGS)

senderManagePackets.o : $(SRC)/senderManagePackets.c $(SRC)/defines.h packets.o timer.o
	$(CC) $(CFLAGS) -c $(SRC)/senderManagePackets.c $(LFLAGS)

receiverManagePackets.o : $(SRC)/receiverManagePackets.c $(SRC)/defines.h packets.o timer.o
	$(CC) $(CFLAGS) -c $(SRC)/receiverManagePackets.c $(LFLAGS)

timer.o : $(SRC)/timer.c
	$(CC) $(CFLAGS) -c $(SRC)/timer.c

clean :
	@rm -f *.o #-f prevents an error message to be displayed if no file were found to be deleted

cleanall : clean
	@rm -f $(PROG_SENDER)
	@rm -f $(PROG_RECEIVER)

tests :
	./tests/tests.sh
