//File receiver.c

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
#include "receiverReadWriteLoop.h"

int main(int argc, char *argv[])
{
	fprintf(stderr, "Running receiver\n");

	//================= Check the validity of the program's arguments ===========================
	if (argc < 3)
	{
		ERROR("Too few arguments");
		fprintf(stderr, "\nReceiver program\nUsage :\n");
		fprintf(stderr, "\treceiver [options] hostname port\n");
		fprintf(stderr, "\thostname :\tthe host IP to connect to\n");
		fprintf(stderr, "\tport :\t\tthe port the messages are received from\n");
		fprintf(stderr, "\toptions :\t-f : the file which the messages have to be written to\n\n");
		return EXIT_FAILURE;
	}

	//====================== Get the program's arguments =============================
	char* fileToWriteName = NULL;

	int opt;
	extern char* optarg;
	while((opt = getopt(argc, argv, "f:")) != -1)
	{
		if (opt == 'f')
			fileToWriteName = optarg;
		else
		{
			ERROR("There was an error while getting the program's options");
			return EXIT_FAILURE;
		}
	}

	char* senderHostname = argv[argc-2];

	char* endptr = NULL;
	int port = strtol(argv[argc-1], &endptr, 10);
	if (*endptr != '\0')
	{
		ERROR("Wrong argument : the port must be an integer number");
		return EXIT_FAILURE;
	}

	//========================= Create connection =====================================
	//--------------- Resolve hostname ----------------------
	struct sockaddr_in6 senderAddress;
	const char* errMsg = real_address(senderHostname, &senderAddress);
	if (errMsg != NULL)
	{
		fprintf(stderr, "Couldn't resolve hostname %s :%s\n", senderHostname, errMsg);
		return EXIT_FAILURE;
	}

	//--------------- Create socket -----------------------
	int sfd = create_receiver_socket(&senderAddress, port);
	if (sfd < 0)
	{
		ERROR("Couldn't create socket");
		return EXIT_FAILURE;
	}

	//-------------- Connect socket after receiving the first packet --------------------
	int err = connect_receiver_socket(sfd);
	if (err == -1)
	{
		ERROR("Couldn't connect receiver to socket after receiving the first packet");
		close(sfd);
		return EXIT_FAILURE;
	}

	//================= Open file to write =========================
	int fileToWriteFd = -1;
	if (fileToWriteName != NULL)
	{
		fileToWriteFd = open(fileToWriteName, O_WRONLY|O_CREAT|O_TRUNC);
		if (fileToWriteFd < 0)
		{
			perror("Couldn't open file to write");
			close(sfd);
			return EXIT_FAILURE;
		}
	}
	else
		fileToWriteFd = fileno(stdout);

	//=========== Main loop (receive and interpret packets and send acknowledments) ============
	//TODO check return value
	receiverReadWriteLoop(sfd, fileToWriteFd);

	//===================== Close all resources =========================
	if (fileToWriteFd != -1)
		close(fileToWriteFd);
	close(sfd);

	return EXIT_SUCCESS;
}
