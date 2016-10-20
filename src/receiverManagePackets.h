#ifndef _RECEIVER_MANAGE_PACKETS_H_
#define _RECEIVER_MANAGE_PACKETS_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "defines.h"
#include "packets.h"

ERR_CODE receiverInterpretDataReceived(const uint8_t* data, int length);

#endif //_RECEIVER_MANAGE_PACKETS_H_
