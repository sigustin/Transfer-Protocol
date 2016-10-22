#ifndef _READ_WRITE_LOOP_H_
#define _READ_WRITE_LOOP_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
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
#include "senderManagePackets.h"
#include "timer.h"

#define TIME_TO_WAIT_WITHOUT_EOF_ACK   RETRANSMISSION_TIME*10

ERR_CODE senderReadWriteLoop(const int sfd, const int inputFile);

#endif //_READ_WRITE_LOOP_H_
