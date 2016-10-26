//File sender.c

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "defines.h"
#include "createConnection.h"
#include "senderReadWriteLoop.h"

//Works with port 1025 and a receiver running on the same port

int main(int argc, char *argv[])
{
	//fprintf(stderr, "Running sender\n");

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
	char* fileToSendName = NULL;

	int opt;
	extern char* optarg;
	while((opt = getopt(argc, argv, "f:")) != -1)
	{
		if (opt == 'f')
			fileToSendName = optarg;
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
		fprintf(stderr, "Couldn't resolve hostname %s : %s\n", receiverHostname, errMsg);
		return EXIT_FAILURE;
	}
	DEBUG("Host name resolved");

	//--------------- Create socket -----------------------
	int sfd = create_sender_socket(&receiverAddress, port);
	if (sfd < 0)
	{
		ERROR("Couldn't create socket");
		return EXIT_FAILURE;
	}

	//================= Open file to send ==================================
	int fileToSendFd = -1;
	if (fileToSendName != NULL)
	{
		fileToSendFd = open(fileToSendName, O_RDONLY);
		if (fileToSendFd < 0)
		{
			perror("Couldn't open file");
			close(sfd);
			return RETURN_FAILURE;
		}
	}
	else
		fileToSendFd = fileno(stdin);

	//=============== Main loop (prepare and send packets and receive acknowledgments) =============
	//TODO check return value
	senderReadWriteLoop(sfd, fileToSendFd);

	//=================== Close all resources ===============================
	if (fileToSendFd != -1)
		close(fileToSendFd);
	close(sfd);

	return EXIT_SUCCESS;
}
