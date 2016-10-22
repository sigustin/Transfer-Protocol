#ifndef _RECEIVER_MANAGE_PACKETS_H_
#define _RECEIVER_MANAGE_PACKETS_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

#include "defines.h"
#include "packets.h"
#include "timer.h"

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
 * Puts data packet in the buffer of out-of-sequence received packets (if seqnum is in window)
 */
ERR_CODE putOutOfSequencePktInBuf(pkt_t* dataPkt);

/*
 * If the buffer containing acknowledments to be sent is not empty,
 * sends the acknowledgments on the socket @sfd
 * Deletes every acknowledgments sent except the last one
 */
ERR_CODE sendAckFromBuffer(const int sfd);

/*
 * Sends the first acknowledgment in the buffer on @sfd
 */
ERR_CODE sendFirstAckFromBuffer(const int sfd);

/*
 * If there's something to write in the buffer containing packets in sequence,
 * writes the payload of the packets in the output file @fd
 */
ERR_CODE writePayloadInOutputFile(const int fd);

/*
 * @return :   true if there's still something to write in the output file or if there are still acknowledgments that have to be sent
 *             false otherwise
 */
bool stillSomethingToWrite();

/*
 * Deletes every packets in @acknowledgmentsToSend, @dataPktInSequence and @bufOutOfSequencePkt
 * Inteded to be called when exiting program
 * WARNING : there's a function with the same signature in senderManagePackets.h/c
 */
void purgeBuffers();

/*
 * Intended only for DEBUG
 * Prints on stderr the content of the buffer @dataPktInSequence
 */
void printDataPktInSequenceBuf();

/*
 * Intended only for DEBUG
 * Prints on stderr the content of the buffer @bufOutOfSequencePkt
 */
void printDataPktOutOfSequenceBuf();

#endif //_RECEIVER_MANAGE_PACKETS_H_
