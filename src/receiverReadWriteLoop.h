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
#include "receiverManagePackets.h"

ERR_CODE receiverReadWriteLoop(const int sfd, const int outputFile);

#endif //_READ_WRITE_LOOP_H_
