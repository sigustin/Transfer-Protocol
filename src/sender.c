//File sender.c

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "defines.h"
#include "createConnection.h"

int main(int argc, char *argv[])
{
	fprintf(stderr, "Running sender\n");

	//================== Check validity of the program's arguments =======================
	if (argc < 3)
	{
		ERROR("Too few arguments");
		fprintf(stderr, "\nSender program\nUsage :\n");
		fprintf(stderr, "\tsender [options] hostname port\n");
		fprintf(stderr, "\thostname :\tthe host IP to send messages\n");
		fprintf(stderr, "\tport :\t\tthe port the messages are sent to\n");
		fprintf(stderr, "\toptions :\t-f : the file which the messages have to be read from\n\n");
		return EXIT_FAILURE;
	}

	//===================== Get the program's arguments ==========================
	char* fileToSend = NULL;

	int opt;
	extern char* optarg;
	while((opt = getopt(argc, argv, "f:")) != -1)
	{
		if (opt == 'f')
			fileToSend = optarg;
		else
		{
			ERROR("There was an error while getting the program's options");
			return EXIT_FAILURE;
		}
	}

	char* receiverHostname = argv[argc-2];

	char* endptr = NULL;
	int port = strtol(argv[argc-1], &endptr, 10);
	if (*endptr != '\0')
	{
		ERROR("Wrong argument : the port must be an integer number");
		return EXIT_FAILURE;
	}

	//======================= Create the connection =============================
	//--------------- Resolve hostname ----------------------
	struct sockaddr_in6 receiverAddress;
	const char* errMsg = real_address(receiverHostname, &receiverAddress);
	if (errMsg != NULL)
	{
		fprintf(stderr, "Couldn't resolve hostname %s :%s\n", receiverHostname, errMsg);
		return EXIT_FAILURE;
	}

	//--------------- Create socket -----------------------
	int sfd = create_sender_socket(&receiverAddress, port);
	if (sfd < 0)
	{
		ERROR("Couldn't create socket");
		return EXIT_FAILURE;
	}

	//=============== Main loop (prepare and send packets and receive acknowledments) =============

	return EXIT_SUCCESS;
}
