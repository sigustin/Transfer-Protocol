#include "createConnection.h"

const char * real_address(const char *address, struct sockaddr_in6 *rval)
{
	DEBUG("real_address");

	if (address == NULL)
		return "Couldn't find address to resolve";

	struct addrinfo* tmpAddrInfo = NULL;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	//hints.ai_flags = AI_PASSIVE;

	int err = getaddrinfo(address, NULL, &hints, &tmpAddrInfo);
	if (err != 0)
	{
		perror("Couldn't resolve address");
		switch (err)
		{
			/*case EAI_ADDRFAMILY:
				return "the specified network host does not have any network addresses in the requested address family.";*/
			case EAI_AGAIN:
				return "the name server returned a temporary failure indication. Try again later.";
			case EAI_BADFLAGS:
				return "hints.ai_flags contains invalid flags ; or, hints.ai_flags included AI_CANONNAME and name was NULL.";
			case EAI_FAIL:
				return "the name server returned a permanent failure indication.";
			case EAI_FAMILY:
				return "the requested address family is not supported.";
			case EAI_MEMORY:
				return "out of memory.";
			/*case EAI_NODATA:
				return "the specified network host exists, but does not have any network addresses defined.";*/
			case EAI_NONAME:
				return "the node or service is not known ;  or both are NULL.";
			case EAI_SERVICE:
				return "the requested sevice is not available for the requesed socket type.";
			case EAI_SOCKTYPE:
				return "the requested socket type is not supported.";
			case EAI_SYSTEM:
				return "other system error occured, check errno for details.";
			default :
				return "an undefined error occured.";
		}
	}

	if (rval == NULL)
		rval = (struct sockaddr_in6*)(tmpAddrInfo)->ai_addr;
	else
		memcpy(rval, (struct sockaddr_in6*) (tmpAddrInfo)->ai_addr, sizeof(struct sockaddr_in6));

	freeaddrinfo(tmpAddrInfo);

	return NULL;
}

int create_sender_socket(struct sockaddr_in6* receiverAddress, int port)
{
	DEBUG("create_sender_socket");

	struct protoent* protocolInfo = getprotobyname("udp");
	int protocol = protocolInfo->p_proto;
	int sfd = socket(AF_INET6, SOCK_DGRAM, protocol);
	if (sfd == -1)
	{
		perror("Couldn't create sender socket");
		return -1;
	}

	receiverAddress->sin6_port = htons(port);
	int err = connect(sfd, (const struct sockaddr*) receiverAddress, sizeof(struct sockaddr_in6));
	if (err == -1)
	{
		perror("Couldn't connect socket to receiver address");
		return -1;
	}

	fprintf(stderr, "DEBUG :\tconnected to port %d (=>%d)\n", port, htons(port));

	return sfd;
}

int create_receiver_socket(struct sockaddr_in6* senderAddress, int port)
{
	DEBUG("create_receiver_socket");

	struct protoent* protocolInfo = getprotobyname("udp");
	int protocol = protocolInfo->p_proto;
	int sfd = socket(AF_INET6, SOCK_DGRAM, protocol);
	if (sfd == -1)
	{
		perror("Couldn't create receiver socket");
		return -1;
	}

	senderAddress->sin6_port = htons(port);
	int err = bind(sfd, (const struct sockaddr*) senderAddress, sizeof(struct sockaddr_in6));
	if (err == -1)
	{
		perror("Couldn't bind socket to sender address");
		return -1;
	}

	fprintf(stderr, "DEBUG :\tbound to port %d (=>%d)\n", port, htons(port));

	return sfd;
}

int connect_receiver_socket(int sfd)
{
	DEBUG("connect_receiver_socket");

	if (sfd < 0)
	{
		ERROR("Invalid file descriptor");
		return -1;
	}

	struct sockaddr_in6 from;
	memset(&from, 0, sizeof(from));
	socklen_t sockaddrSize = sizeof(struct sockaddr_in6);
	int8_t buf[MAX_PKT_SIZE];
	int bytesRead;
	if ((bytesRead = recvfrom(sfd, buf, MAX_PKT_SIZE, MSG_PEEK, (struct sockaddr*) &from, &sockaddrSize)) == -1)
	{
		ERROR("Couldn't receive the first packet that was sent");
		return -1;
	}

	int err = connect(sfd, (struct sockaddr*) &from, sizeof(struct sockaddr_in6));
	if (err != 0)
	{
		ERROR("Couldn't connect receiver socket to sender address");
		return -1;
	}

	return 0;
}
