#ifndef _CREATE_CONNECTION_H_
#define _CREATE_CONNECTION_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "defines.h"

/* Resolve the resource name to an usable IPv6 address
 * @address: The name to resolve
 * @rval: Where the resulting IPv6 address descriptor should be stored
 * @return: NULL if it succeeded, or a pointer towards
 *          a string describing the error if any.
 *          (const char* means the caller cannot modify or free the return value,
 *           so do not use malloc!)
 */
const char * real_address(const char *address, struct sockaddr_in6 *rval);

/*
 * Create a socket and initialize it
 * @return :   a file descriptor number representing the socket
 *             -1 in case of error
 */
int create_sender_socket(struct sockaddr_in6* receiverAddress, int port);
int create_receiver_socket(struct sockaddr_in6* senderAddress, int port);

/* Block the caller until a message is received on sfd,
 * and connect the socket to the source address of the received message.
 * @sfd: a file descriptor to a bound socket but not yet connected
 * @return: 0 in case of success, -1 otherwise
 * @POST: This call is idempotent, it does not 'consume' the data of the message,
 * and could be repeated several times blocking only at the first call.
 */
int connect_receiver_socket(int sfd);

#endif //_CREATE_CONNECTION_H_
