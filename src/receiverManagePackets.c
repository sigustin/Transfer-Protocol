#include "receiverManagePackets.h"

extern bool EOFPktReceived;

pkt_t* acknowledgmentsToSend[MAX_PACKETS_PREPARED] = {NULL};//buffer containing the acknowledments to be send by receiverReadWriteLoop
int indexFirstAckToSend = 0, nbAckToSend = 0;
struct timeval timerAckSent;
int lastAckSeqnum = -1;

pkt_t* dataPktInSequence[MAX_PACKETS_PREPARED] = {NULL};//will contain the packets received in sequence and that have to be written on the output file
int indexFirstDataPkt = 0, nbDataPktToWrite = 0;

bool hasFirstPktBeenReceived = false;
uint8_t lastSeqnumReceivedInOrder = 0;
uint32_t lastTimestampReceived = 0;

/*
 * The next buffer (for out-of-sequence packets) works as follows :
 * @firstPktBufIndex contains the index of the packet with seqnum == nextSeqnumToBeReceived+1
 * Then, when an out-of-sequence packet arrives, it is placed according to its seqnum.
 * When an in-sequence packet arrives, @firstPktBufIndex is updated and we check if we can't move a packet
 * from this buffer to the buffer of packets to write in the output file in the next loop.
 * EXAMPLE :   say the next seqnum that should arrive is 10,
 *             say we have already received packet with seqnum 13 and 15
 *             say we receive packet with seqnum 12
 *             say the buffer for out-of-sequence packets has a size of 8
 *
 *                Before we add packet #12, the buffer should look like :
 *                      x x X x 13 x 15 x       where @firstPktBufIndex is the index of X (x and X represent NULL here)
 *
 *                After we add packet #12, the buffer should look like :
 *                      x x X 12 13 x 15 x      (@firstPktBufIndex hasn't changed)
 *
 *                If we now receive packet #10, @firstPktBufIndex will be incremented
 *                to be the index of packet #12 in the buffer.
 *                If we then receive packet #11 (which is the next that should be received),
 *                it is accepted as the next packet in sequence and, as any time an in-sequence packet
 *                is received, we check if the packet at index @firstPktBufIndex in the out-of-sequence
 *                buffer is not NULL.
 *                Here packet #12 is present in the buffer, we can then switch it to the buffer of
 *                things to write in the output file. We can then do the same thing for packet #13
 *                which is already in the out-of-sequence buffer, but not for packet #14 which is not
 *                (because it hasn't been received).
 *
 * By using this type of buffer, we improve the speed of the program since it doesn't have to make a for loop
 * everytime an in-sequence packet is received to check if the next packet is not in the out-of-sequence buffer.
 * Moreover, this way we are sure the out-of-sequence buffer can't be filled with packets with a seqnum very far
 * from the next that should be received (in case of a network with a very high drop rate).
 *
 * To test if a packet as a valid seqnum to be added in this buffer and to add it, we use @putOutOfSequencePktInBuf.
 * To test if we can move a packet from this buffer to the buffer of things to write in the output file in the next loop,
 * we use @checkOutOfSequencePkt.
 *
 * Note that the size of the window correspond to the number of places left in that buffer, meaning that
 * the sender shouldn't try to send more packets than the number of places in the buffer (though if these packets
 * are out of the window, they will be discarded and the window won't shrink).
 *
 */
pkt_t* bufOutOfSequencePkt[MAX_WINDOW_SIZE] = {NULL};//Contains out-of-sequence received packets
int firstPktBufIndex = 0, nbPktOutOfSequenceInBuf = 0;//window size = MAX_WINDOW_SIZE-nbPktOutOfSequenceInBuf

ERR_CODE receiveDataPacket(const uint8_t* data, int length)
{
   if (length <= 0)
   {
      ERROR("Tried to interpret data buffer with no length");
      return RETURN_FAILURE;
   }

   //------------ Decode data --------------------
   pkt_t* pktReceived = pkt_new();
   pkt_status_code errCode;
   errCode = pkt_decode(data, length, pktReceived);
   if (errCode != PKT_OK)//Invalid pkt
   {
      switch (errCode)
      {
         case E_INCONSISTENT:
            ERROR("Received inconsistent packet");
            break;
         case E_NOHEADER:
            ERROR("Received packet with invalid header");
            break;
         case E_LENGTH:
            ERROR("Received packet with invalid length");
            break;
         case E_TYPE:
            ERROR("Received packet with invalid type");
            break;
         case E_CRC:
            ERROR("Received packet with wrong CRC");
            break;
         default:
            ERROR("Error while decoding data");
            break;
      }

      //TODO ask to send the packet again or ignore?
      pkt_del(pktReceived);
      return RETURN_FAILURE;
   }
   else//Valid pkt
   {
      if (pkt_get_type(pktReceived) == PTYPE_ACK)
      {
         ERROR("Received acknowledgment from sender");
      }
      else if (pkt_get_length(pktReceived) > MAX_PAYLOAD_SIZE)//ignore packet
      {}
      else//Valid data pkt
      {
         lastTimestampReceived = pkt_get_timestamp(pktReceived);

         //------ Check if packet is in sequence ------------------
         uint8_t seqnum = pkt_get_seqnum(pktReceived);
         fprintf(stderr, "Interpreting valid packet #%d while next seqnum to receive : %d \n", seqnum, (lastSeqnumReceivedInOrder+1)%NB_DIFFERENT_SEQNUM);
         if (!hasFirstPktBeenReceived && (seqnum == 0))//first packet received with seqnum == 0
         {
            if (nbDataPktToWrite+1 < MAX_PACKETS_PREPARED)
            {
               DEBUG_FINE("First data packet received (in sequence) -> putting it in buffer to write in output file");

               hasFirstPktBeenReceived = true;
               lastSeqnumReceivedInOrder = 0;

               int nextIndex = (indexFirstDataPkt+nbDataPktToWrite)%MAX_PACKETS_PREPARED;
               dataPktInSequence[nextIndex] = pktReceived;
               nbDataPktToWrite++;

               //Check if next packets have already been received
               checkOutOfSequencePkt();
               firstPktBufIndex++;
               DEBUG("Out check 1");
            }
            else
            {
               ERROR("Received in sequence packet while buffer of packets to write is full");
            }
         }
         else if ((lastSeqnumReceivedInOrder+1)%NB_DIFFERENT_SEQNUM == seqnum)//packet is in sequence
         {
            if (nbDataPktToWrite+1 < MAX_PACKETS_PREPARED)
            {
               DEBUG_FINE("Data packet in sequence -> putting it in buffer to write in output file");

               lastSeqnumReceivedInOrder++;//255++ == 0 since the result of the calculation is cast in a uint8_t

               int nextIndex = (indexFirstDataPkt+nbDataPktToWrite)%MAX_PACKETS_PREPARED;
               dataPktInSequence[nextIndex] = pktReceived;
               nbDataPktToWrite++;

               //Updating out-of-sequence buffer
               checkOutOfSequencePkt();
               firstPktBufIndex++;
               DEBUG("Out check 2");
            }
            else
            {
               ERROR("Received in sequence packet while buffer of packets to write is full");
            }
         }
         else//packet is out-of-sequence
         {
            DEBUG_FINE("Data packet out-of-sequence -> putting it in buffer out-of-sequence");
            fprintf(stderr, "last seqnum received in sequence : %d\tnext to be received : %d\treceived : %d (ptr : %p)\n", lastSeqnumReceivedInOrder, (lastSeqnumReceivedInOrder+1)%NB_DIFFERENT_SEQNUM, seqnum, pktReceived);
            //---------- Check if seqnum is in the window and put it in the buffer if it is ------------------
            //TODO check return value
            putOutOfSequencePktInBuf(pktReceived);

            printDataPktOutOfSequenceBuf();
         }

         if (nbAckToSend+1 < MAX_PACKETS_PREPARED)
         {
            //--------- Prepare ack and put it in buffer to send ----------------------
            pkt_t* newAck = createNewAck();
            if (newAck == NULL)
            {
               ERROR("Couldn't create new acknowledgment");
               return RETURN_FAILURE;
            }

            acknowledgmentsToSend[(indexFirstAckToSend+nbAckToSend)%MAX_PACKETS_PREPARED] = newAck;
            nbAckToSend++;
         }
         else
         {
            ERROR("Tried to make new acknowledgment while buffer of acknowledgments to send is full");
            //remove first ack from list or wait for them to be sent?
         }
      }
   }

   return RETURN_SUCCESS;
}

pkt_t* createNewAck()
{
   pkt_t* ack = pkt_new();
   if (ack == NULL)
   {
      ERROR("Couldn't allocate new acknowledgment packet");
      return NULL;
   }

   //TODO check return values
   pkt_set_type(ack, PTYPE_ACK);
   pkt_set_window(ack, (uint8_t) MAX_WINDOW_SIZE-nbPktOutOfSequenceInBuf);
   pkt_set_seqnum(ack, (lastSeqnumReceivedInOrder+1)%NB_DIFFERENT_SEQNUM);//the seqnum of the next packet that the sender must send
   pkt_set_length(ack, 0);
   pkt_set_timestamp(ack, lastTimestampReceived);
   pkt_set_payload(ack, NULL, 0);//sets crc

   return ack;
}

ERR_CODE putOutOfSequencePktInBuf(pkt_t* dataPkt)
{
   DEBUG_FINE("putOutOfSequencePktInBuf");

   if (dataPkt == NULL)
   {
      ERROR("Tried to put no data packet in buffer of received packets");
      return RETURN_FAILURE;
   }
   if (nbPktOutOfSequenceInBuf == MAX_WINDOW_SIZE)//no place in buffer
   {
      ERROR("Tried to put data packet in full buffer");
      return RETURN_FAILURE;
   }

   //------ Compute distance between the first seqnum that should be received to make the window move -----
   uint8_t firstSeqnumInBuf = (lastSeqnumReceivedInOrder+2)%NB_DIFFERENT_SEQNUM, seqnum = pkt_get_seqnum(dataPkt);
   int distanceSeqnumToFirstWindowSeqnum;
   if (seqnum > firstSeqnumInBuf)
      distanceSeqnumToFirstWindowSeqnum = seqnum - firstSeqnumInBuf;
   else
      distanceSeqnumToFirstWindowSeqnum = (NB_DIFFERENT_SEQNUM - firstSeqnumInBuf) + seqnum;

   //----- Put packet at the right place in the buffer -------
   if (distanceSeqnumToFirstWindowSeqnum > MAX_WINDOW_SIZE)//out of buffer
   {
      WARNING("Received packet out of the window of reception");
      fprintf(stderr, "distance from next to be received : %d\n", distanceSeqnumToFirstWindowSeqnum);
   }//discard
   else
   {
      int index = (firstPktBufIndex + distanceSeqnumToFirstWindowSeqnum) % MAX_WINDOW_SIZE;
      if (bufOutOfSequencePkt[index] != NULL)
      {
         WARNING("Received out-of-sequence packet already in buffer");
      }
      else
      {
         bufOutOfSequencePkt[index] = dataPkt;
         nbPktOutOfSequenceInBuf++;
      }
   }

   return RETURN_SUCCESS;
}

ERR_CODE sendAckFromBuffer(const int sfd)
{
   //Initializing timer
   static bool firstSending = true;
   if (firstSending)
   {
      timerAckSent.tv_sec = timerAckSent.tv_usec = 0;
      firstSending = false;
   }

   DEBUG_FINEST("Try to send ack");

   if (sfd < 0)
   {
      ERROR("Invalid socket file descriptor");
      return RETURN_FAILURE;
   }

   if (nbAckToSend > 1)
   {
      //BUG sent once too many times (ack #x has been sent -maybe multiple times- and must be sent again before being deleted)
      //send all acknowledgments and remove them except the last one
      while (nbAckToSend >= 1)
      {
         DEBUG_FINE("Send acknowledgment");

         if (lastAckSeqnum != pkt_get_seqnum(acknowledgmentsToSend[indexFirstAckToSend]))
         {
            if (sendFirstAckFromBuffer(sfd) != RETURN_SUCCESS)
               return RETURN_FAILURE;

            //reset timer
            gettimeofday(&timerAckSent, NULL);
         }

         if (nbAckToSend == 1)
            break;

         if (nbAckToSend > 1)
         {
            pkt_del(acknowledgmentsToSend[indexFirstAckToSend]);
            acknowledgmentsToSend[indexFirstAckToSend] = NULL;
            indexFirstAckToSend++;
            nbAckToSend--;
         }
      }
   }
   else//nbAckToSend == 1
   {
      if (isTimerOver(timerAckSent))//true also with a timer zeroed out
      {
         DEBUG_FINE("Send acknowledgment");

         if (sendFirstAckFromBuffer(sfd) != RETURN_SUCCESS)
            return RETURN_FAILURE;

         //reset timer
         gettimeofday(&timerAckSent, NULL);
      }
   }

   if (EOFPktReceived)//last acknowledgment has been sent at least once
   {
      //delete last acknowledgment
      pkt_del(acknowledgmentsToSend[indexFirstAckToSend]);
      acknowledgmentsToSend[indexFirstAckToSend] = NULL;
      indexFirstAckToSend++;
      nbAckToSend--;
   }

   return RETURN_SUCCESS;
}

ERR_CODE sendFirstAckFromBuffer(const int sfd)
{
   if (sfd < 0)
   {
      ERROR("Invalid socket file descriptor");
      return RETURN_FAILURE;
   }
   if (nbAckToSend <= 0)
   {
      ERROR("Tried to send no acknowledgment");
      return RETURN_FAILURE;
   }

   uint8_t* rawAckToSend = malloc(HEADER_SIZE);//no payload in acknowledments
   size_t lengthAck = HEADER_SIZE;
   //TODO check return value
   pkt_encode(acknowledgmentsToSend[indexFirstAckToSend], rawAckToSend, &lengthAck);
   if (lengthAck != HEADER_SIZE)
   {
      ERROR("Couldn't encode the full acknowledment to be sent");
      free(rawAckToSend);
      return RETURN_FAILURE;
   }

   fprintf(stderr, "Acknowledgment has seqnum : %d\n", pkt_get_seqnum(acknowledgmentsToSend[indexFirstAckToSend]));

   if (sendto(sfd, rawAckToSend, lengthAck, 0, NULL, 0) != lengthAck)
   {
      ERROR("Couldn't write acknowledment on socket");
      free(rawAckToSend);
      return RETURN_FAILURE;
   }

   lastAckSeqnum = pkt_get_seqnum(acknowledgmentsToSend[indexFirstAckToSend]);

   free(rawAckToSend);

   return RETURN_SUCCESS;
}

ERR_CODE writePayloadInOutputFile(const int fd)
{
   if (fd < 0)
   {
      ERROR("Invalid file descriptor");
      return RETURN_FAILURE;
   }

   while (nbDataPktToWrite > 0)
   {
      DEBUG_FINE("Writing payload in the output file");

      if (pkt_get_length(dataPktInSequence[indexFirstDataPkt]) == 0)
      {
         DEBUG("Received last packet");
         EOFPktReceived = true;
      }
      else
      {
         const uint8_t* payload = pkt_get_payload(dataPktInSequence[indexFirstDataPkt]);
         size_t length = pkt_get_length(dataPktInSequence[indexFirstDataPkt]);
         if (write(fd, payload, length) == -1)
         {
            perror("Couldn't write payload in output file");
            return RETURN_FAILURE;
         }
      }

      //Remove written packet from buffer
      pkt_del(dataPktInSequence[indexFirstDataPkt]);
      dataPktInSequence[indexFirstDataPkt] = NULL;

      indexFirstDataPkt++;
      indexFirstDataPkt %= MAX_PACKETS_PREPARED;
      nbDataPktToWrite--;
   }

   return RETURN_SUCCESS;
}

void checkOutOfSequencePkt()
{
   DEBUG_FINE("checkOutOfSequencePkt");

   printDataPktOutOfSequenceBuf();

   if (nbPktOutOfSequenceInBuf <= 0)
      return;

   fprintf(stderr, "nbPktOutOfSequenceInBuf : %d\tfirst out of sequence : %p\n", nbPktOutOfSequenceInBuf, bufOutOfSequencePkt[firstPktBufIndex]);

   while ((bufOutOfSequencePkt[firstPktBufIndex] != NULL) && (nbDataPktToWrite+1 < MAX_PACKETS_PREPARED))
   {
      lastSeqnumReceivedInOrder++;//equivalent to lastSeqnumReceivedInOrder = pkt_get_seqnum(bufOutOfSequencePkt[firstPktBufIndex]);

      fprintf(stderr, "Taking pkt #%d out of out-of-sequence buffer\n", lastSeqnumReceivedInOrder);

      //put this packet in the buffer of in-sequence packets (to write in the output file)
      int nextIndex = (indexFirstDataPkt+nbDataPktToWrite)%MAX_PACKETS_PREPARED;
      dataPktInSequence[nextIndex] = bufOutOfSequencePkt[firstPktBufIndex];
      nbDataPktToWrite++;

      //remove packet from out-of-sequence buffer
      bufOutOfSequencePkt[firstPktBufIndex] = NULL;
      firstPktBufIndex++;
      firstPktBufIndex %= MAX_WINDOW_SIZE;
      nbPktOutOfSequenceInBuf--;
      if (nbPktOutOfSequenceInBuf)//shouldn't be possible to happen
      {
         ERROR("Number of packets in the out-of-sequence buffer is negative");
      }

      printDataPktOutOfSequenceBuf();

      fprintf(stderr, "nbPktOutOfSequenceInBuf : %d\tfirst out of sequence : %p\n", nbPktOutOfSequenceInBuf, bufOutOfSequencePkt[firstPktBufIndex]);
   }
}

bool stillSomethingToWrite()
{
   return (nbDataPktToWrite > 0) || (nbAckToSend > 0);
}

void purgeBuffers()
{
   if (nbAckToSend > 0)
   {
      WARNING("Purging not empty buffer of acknowledgments to send");
   }
   if (nbDataPktToWrite > 0)
   {
      WARNING("Purging not empty buffer of data packets to write on output file");
   }
   if (nbPktOutOfSequenceInBuf > 0)
   {
      WARNING("Purging not empty buffer of out-of-sequence data packets");
   }

   int i;
   for (i=0; i<MAX_PACKETS_PREPARED; i++)
   {
      if (acknowledgmentsToSend[i] != NULL)
         pkt_del(acknowledgmentsToSend[i]);
      if (dataPktInSequence[i] != NULL)
         pkt_del(dataPktInSequence[i]);
   }
   for (i=0; i<MAX_WINDOW_SIZE; i++)
   {
      if (bufOutOfSequencePkt[i] != NULL)
         pkt_del(bufOutOfSequencePkt[i]);
   }
}

void printDataPktInSequenceBuf()
{
   if (nbDataPktToWrite <= 0)
   {
      DEBUG_FINE("Buffer of packets to write is empty");
      return;
   }

   fprintf(stderr, "Buffer of packets to write :\n");
   int i, index;
   for (i=0, index=indexFirstDataPkt; i<nbDataPktToWrite; i++, index++)
   {
      if (index == MAX_PACKETS_PREPARED)
         index = 0;
      fprintf(stderr, "\tPacket #%d\n", i);
      fprintf(stderr, "\t\ttype : %d\n", pkt_get_type(dataPktInSequence[index]));
      fprintf(stderr, "\t\twindow : %d\n", pkt_get_window(dataPktInSequence[index]));
      fprintf(stderr, "\t\tseqnum : %d\n", pkt_get_seqnum(dataPktInSequence[index]));
      fprintf(stderr, "\t\tlength : %d\n", pkt_get_length(dataPktInSequence[index]));
      fprintf(stderr, "\t\ttimestamp : %d\n", pkt_get_timestamp(dataPktInSequence[index]));
      fprintf(stderr, "\t\tcrc : %d\n", pkt_get_crc(dataPktInSequence[index]));
   }
}

void printDataPktOutOfSequenceBuf()
{
   if (nbPktOutOfSequenceInBuf <= 0)
   {
      DEBUG_FINE("Buffer of packets out-of-sequence is empty");
      return;
   }

   fprintf(stderr, "Seqnums in buffer of packets out-of-sequence (%d packets) :\t\n", nbPktOutOfSequenceInBuf);
   int i, index;
   for (i=0, index=firstPktBufIndex; i<nbPktOutOfSequenceInBuf; i++, index++)
   {
      if (index == MAX_WINDOW_SIZE)
         index = 0;
      fprintf(stderr, "%d ", pkt_get_seqnum(bufOutOfSequencePkt[index]));
      /*
      fprintf(stderr, "\tPacket #%d (ptr : %p) (index %d)\n", i, bufOutOfSequencePkt[index], index);
      fprintf(stderr, "\t\ttype : %d\n", pkt_get_type(bufOutOfSequencePkt[index]));
      fprintf(stderr, "\t\twindow : %d\n", pkt_get_window(bufOutOfSequencePkt[index]));
      fprintf(stderr, "\t\tseqnum : %d\n", pkt_get_seqnum(bufOutOfSequencePkt[index]));
      fprintf(stderr, "\t\tlength : %d\n", pkt_get_length(bufOutOfSequencePkt[index]));
      fprintf(stderr, "\t\ttimestamp : %d\n", pkt_get_timestamp(bufOutOfSequencePkt[index]));
      fprintf(stderr, "\t\tcrc : %d\n", pkt_get_crc(bufOutOfSequencePkt[index]));*/
   }
   fprintf(stderr, "\n");
}
