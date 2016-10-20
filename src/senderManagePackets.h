#ifndef _SENDER_MANAGE_PACKETS_H_
#define _SENDER_MANAGE_PACKETS_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "defines.h"
#include "packets.h"

//--------------------------
//Those functions are called by the sender only
//--------------------------

/*
 * Decodes the data into a packet, checks if the packet is a valid acknowledgment and updates global values
 */
ERR_CODE senderInterpretDataReceived(const uint8_t* data, int length);

#endif //_SENDER_MANAGE_PACKETS_H_
