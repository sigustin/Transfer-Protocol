#include "createConnection.h"

const char * real_address(const char *address, struct sockaddr_in6 *rval)
{
	if (address == NULL)
		return "Couldn't find address to resolve";

	struct addrinfo* tmpAddrInfo = NULL;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	int err = getaddrinfo(address, NULL, &hints, &tmpAddrInfo);
	if (err != 0)
	{
		perror("Couldn't resolve address");
		return "Couldn't resolve address";
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

	return sfd;
}

int create_receiver_socket(struct sockaddr_in6* senderAddress, int port)
{
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

	return sfd;
}

int connect_receiver_socket(int sfd)
{
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
