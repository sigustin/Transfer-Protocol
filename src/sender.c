//File sender.c

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "defines.h"

int main(int argc, char *argv[])
{
	fprintf(stderr, "Running sender\n");

	if (argc < 3)
	{
		ERROR("Too few arguments");
		fprintf(stderr, "\nSender program\nUsage :\n");
		fprintf(stderr, "\tsender [options] hostname port\n");
		fprintf(stderr, "\thostname :\tthe host IP to send messages\n");
		fprintf(stderr, "\tport :\tthe port the messages are sent to\n");
		fprintf(stderr, "\toptions :\tf\tthe file which the messages have to be read from\n\n");
	}

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

	return EXIT_SUCCESS;
}
