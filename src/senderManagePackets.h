#ifndef _SENDER_MANAGE_PACKETS_H_
#define _SENDER_MANAGE_PACKETS_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "defines.h"
#include "packets.h"

#define MAX_PACKETS_PREPARED  1024

//--------------------------
//Those functions are called by the sender only
//--------------------------

/*
 * Decodes the data into a packet, checks if the packet is a valid acknowledgment and updates global values
 */
ERR_CODE senderInterpretDataReceived(const uint8_t* data, uint16_t length);

/*
 * Creates a new data packet
 * @payload : an array containing the payload of the data packet
 * @length : length of the array @payload
 * @return :   a pointer to a new pkt_t (HAS TO BE FREED)
 *             NULL if there was an error
 */
pkt_t* createDataPkt(const uint8_t* payload, uint16_t length);

/*
 * Puts @dataPkt in the buffer of packets ready to be send
 * @return :   RETURN_SUCCESS if there was no error
 *             RETURN_FAILURE if the buffer was full
 */
ERR_CODE putNewPktInBufferToSend(pkt_t* dataPkt);

/*
 * Tries to send a data packet on the socket (if there is something to send in the buffer)
 * @return :   RETURN_SUCCESS if nothing was in the buffer, or a data packet was sent successfully
 *             RETURN_FAILURE if there was an error while sending the data packet
 */
ERR_CODE sendDataPktFromBuffer(const int sfd);

#endif //_SENDER_MANAGE_PACKETS_H_
