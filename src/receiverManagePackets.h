#ifndef _RECEIVER_MANAGE_PACKETS_H_
#define _RECEIVER_MANAGE_PACKETS_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "defines.h"
#include "packets.h"

/*
 * Decodes data received, put it in a pkt_t and checks if it is out-of-sequence
 * Makes a new acknowledment packet according to what was received
 * @data : the buffer of data received
 * @length : the length of the buffer
 * @return :   RETURN_SUCCESS if there was no eerror
 *             RETURN_FAILURE otherwise
 */
ERR_CODE receiveDataPacket(const uint8_t* data, int length);

/*
 * Creates a new acknowledment packet with current window size and next seqnum to be received
 * @return :   the new packet created
 *             NULL if there was an error
 */
pkt_t* createNewAck();

/*
 * Puts @ack in the buffer of acknowledment ready to be sent
 * @return :   RETURN_SUCCESS if there was no error
 *             RETURN_FAILURE otherwise
 */
ERR_CODE putAckInBuf(pkt_t* ack);

/*
 * Puts data packet in the buffer of out-of-sequence received packets (if seqnum is in window)
 */
ERR_CODE putReceivedPktInBuf(pkt_t* dataPkt);

#endif //_RECEIVER_MANAGE_PACKETS_H_
