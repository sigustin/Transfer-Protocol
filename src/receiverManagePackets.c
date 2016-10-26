#include "receiverManagePackets.h"

extern bool EOFPktReceived;

pkt_t* acknowledgmentToSend = NULL;//The next acknowledgment to be send by receiverReadWriteLoop (can be overwritten by a more recent one)
struct timeval timerAckSent;
bool newAckToSend = false;//Send @acknowledgmentToSend when this is true (a new acknowledgment can be send and hasn't been sent yet)

pkt_t* dataPktInSequence[MAX_PACKETS_PREPARED] = {NULL};//will contain the packets received in sequence and that have to be written on the output file
int indexFirstDataPkt = 0, nbDataPktToWrite = 0;

bool hasFirstPktBeenReceived = false;
uint8_t lastSeqnumReceivedInOrder = 0;
uint32_t lastTimestampReceived = 0;

/*
 * The next buffer (for out-of-sequence packets) works as follows :
 * @indexFirstOutOfSequencePkt contains the index of the packet with seqnum == nextSeqnumToBeReceived+1
 * Then, when an out-of-sequence packet arrives, it is placed according to its seqnum.
 * When an in-sequence packet arrives, @indexFirstOutOfSequencePkt is updated and we check if we can't move a packet
 * from this buffer to the buffer of packets to write in the output file in the next loop.
 * EXAMPLE :   say the next seqnum that should arrive is 10,
 *             say we have already received packet with seqnum 13 and 15
 *             say we receive packet with seqnum 12
 *             say the buffer for out-of-sequence packets has a size of 8
 *
 *                Before we add packet #12, the buffer should look like :
 *                      x x X x 13 x 15 x       where @indexFirstOutOfSequencePkt is the index of X (x and X represent NULL here)
 *
 *                After we add packet #12, the buffer should look like :
 *                      x x X 12 13 x 15 x      (@indexFirstOutOfSequencePkt hasn't changed)
 *
 *                If we now receive packet #10, @indexFirstOutOfSequencePkt will be incremented
 *                to be the index of packet #12 in the buffer.
 *                If we then receive packet #11 (which is the next that should be received),
 *                it is accepted as the next packet in sequence and, as any time an in-sequence packet
 *                is received, we check if the packet at index @indexFirstOutOfSequencePkt in the out-of-sequence
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
 */
pkt_t* bufOutOfSequencePkt[MAX_WINDOW_SIZE] = {NULL};//Contains out-of-sequence received packets
int indexFirstOutOfSequencePkt = 0, nbPktOutOfSequenceInBuf = 0;//window size = MAX_WINDOW_SIZE-nbPktOutOfSequenceInBuf

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

      //discard packet
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
         fprintf(stderr, "DEBUG : Interpreting valid packet #%d while next seqnum to receive : %d \n", seqnum, (lastSeqnumReceivedInOrder+1)%NB_DIFFERENT_SEQNUM);
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
               indexFirstOutOfSequencePkt++;
               indexFirstOutOfSequencePkt %= MAX_WINDOW_SIZE;
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
               indexFirstOutOfSequencePkt++;
               indexFirstOutOfSequencePkt %= MAX_WINDOW_SIZE;
            }
            else
            {
               ERROR("Received in sequence packet while buffer of packets to write is full");
            }
         }
         else//packet is out-of-sequence
         {
            DEBUG_FINE("Data packet out-of-sequence -> putting it in buffer out-of-sequence");
            //fprintf(stderr, "last seqnum received in sequence : %d\tnext to be received : %d\treceived : %d (ptr : %p)\n", lastSeqnumReceivedInOrder, (lastSeqnumReceivedInOrder+1)%NB_DIFFERENT_SEQNUM, seqnum, pktReceived);
            //---------- Check if seqnum is in the window and put it in the buffer if it is ------------------
            if (putOutOfSequencePktInBuf(pktReceived) != RETURN_SUCCESS)
            {
               ERROR("Couldn't put packet in out-of-sequence buffer");
               pkt_del(pktReceived);
               return RETURN_FAILURE;
            }

            //printDataPktOutOfSequenceBuf();
         }

         //--------- Prepare new ack to send ----------------------
         pkt_t* newAck = createNewAck();
         if (newAck == NULL)
         {
            ERROR("Couldn't create new acknowledgment");
            return RETURN_FAILURE;
         }

         if (acknowledgmentToSend != NULL)
            pkt_del(acknowledgmentToSend);
         acknowledgmentToSend = newAck;

         //Reset timer
         gettimeofday(&timerAckSent, NULL);

         newAckToSend = true;

         //fprintf(stderr, "Ack prepared with seqnum : %d\n", pkt_get_seqnum(acknowledgmentToSend));
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

   //Making the new acknowledgment packet and checking for err values
   pkt_status_code err;
   if ((err = pkt_set_type(ack, PTYPE_ACK)) != PKT_OK)
   {
      ERROR("Couldn't set new acknowledgment's type");
      switch (err)
      {
         case E_INCONSISTENT:
            ERROR("Packet was inconsistent");
            break;
         case E_TYPE:
            ERROR("Tried to set to invalid type");
            break;
         default:
            ERROR("An undefined error occured");
            break;
      }
      pkt_del(ack);
      return NULL;
   }
   if ((err = pkt_set_window(ack, (uint8_t) MAX_WINDOW_SIZE-nbPktOutOfSequenceInBuf)) != PKT_OK)
   {
      ERROR("Couldn't set new acknowledgment's window");
      switch (err)
      {
         case E_INCONSISTENT:
            ERROR("Packet is inconsistent");
            break;
         case E_WINDOW:
            ERROR("Tried to set the window to a too big size");
            break;
         default:
            ERROR("An undefined error occured");
            break;
      }
      pkt_del(ack);
      return NULL;
   }
   if ((err = pkt_set_seqnum(ack, (lastSeqnumReceivedInOrder+1)%NB_DIFFERENT_SEQNUM)) != PKT_OK)//the seqnum of the next packet that the sender must send
   {
      ERROR("Couldn't set new acknowledgment's sequence number");
      switch (err)
      {
         case E_INCONSISTENT:
            ERROR("Packet is inconsistent");
            break;
         default:
            ERROR("An undefined error occured");
            break;
      }
      pkt_del(ack);
      return NULL;
   }
   if ((err = pkt_set_length(ack, 0)) != PKT_OK)
   {
      ERROR("Couldn't set new acknowledgment's payload length");
      switch (err)
      {
         case E_INCONSISTENT:
            ERROR("Packet is inconsistent");
            break;
         case E_LENGTH:
            ERROR("Tried to set payload length to a too big length");
            break;
         default:
            ERROR("An undefined error occured");
            break;
      }
      pkt_del(ack);
      return NULL;
   }
   if ((err = pkt_set_timestamp(ack, lastTimestampReceived)) != PKT_OK)
   {
      ERROR("Couldn't set new acknowledgment's timestamp");
      switch (err)
      {
         case E_INCONSISTENT:
            ERROR("Packet is inconsistent");
            break;
         default:
            ERROR("An undefined error occured");
            break;
      }
      pkt_del(ack);
      return NULL;
   }
   if ((err = pkt_set_payload(ack, NULL, 0)) != PKT_OK)//sets crc
   {
      ERROR("Couldn't set acknowledgment's crc");
      switch (err)
      {
         case E_INCONSISTENT:
            ERROR("Packet is inconsistent")
            break;
         case E_LENGTH:
            ERROR("Tried to set a payload with a too big length");
            break;
         default:
            ERROR("An undefined error occured");
            break;
      }
      pkt_del(ack);
      return NULL;
   }

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
   if (seqnum >= firstSeqnumInBuf)
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
      int index = (indexFirstOutOfSequencePkt + distanceSeqnumToFirstWindowSeqnum) % MAX_WINDOW_SIZE;
      //fprintf(stderr, "indexFirstOutOfSequencePkt : %d\tindex added : %d\n", indexFirstOutOfSequencePkt, index);
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

ERR_CODE sendAckIfPossible(const int sfd)
{
   //Initializing timer
   static bool firstSending = true;
   if (firstSending)
   {
      timerAckSent.tv_sec = timerAckSent.tv_usec = 0;
      firstSending = false;
   }

   if (sfd < 0)
   {
      ERROR("Invalid socket file descriptor");
      return RETURN_FAILURE;
   }

   if (newAckToSend || isTimerOver(timerAckSent))//true also with a timer zeroed out
   {
      DEBUG_FINE("Timer over : send acknowledgment");

      if (sendAck(sfd) != RETURN_SUCCESS)
      {
         ERROR("Couldn't send acknowledgment");
         return RETURN_FAILURE;
      }

      newAckToSend = false;

      if (EOFPktReceived)
      {
         DEBUG_FINE("Sending and deleting last acknowledgment");

         //delete acknowledgment
         pkt_del(acknowledgmentToSend);
         acknowledgmentToSend = NULL;
      }
   }

   return RETURN_SUCCESS;
}

ERR_CODE sendAck(const int sfd)
{
   if (sfd < 0)
   {
      ERROR("Invalid socket file descriptor");
      return RETURN_FAILURE;
   }
   if (acknowledgmentToSend == NULL)
   {
      ERROR("Tried to send no acknowledgment on socket");
      return RETURN_FAILURE;
   }

   //--------------- Encode raw acknowledgment to be sent --------------------------
   uint8_t* rawAckToSend = malloc(HEADER_SIZE);//no payload in acknowledgments
   size_t lengthAck = HEADER_SIZE;

   pkt_status_code err;
   if ((err = pkt_encode(acknowledgmentToSend, rawAckToSend, &lengthAck)) != PKT_OK)
   {
      //Check return value
      ERROR("Couldn't encode the acknowledgment to be sent");
      switch (err)
      {
         case E_INCONSISTENT:
            ERROR("Packet is inconsistent");
            break;
         case E_NOMEM:
            ERROR("Tried to encode the acknowledgment to send in a too small buffer");
            break;
         default:
            ERROR("An undefined error occured");
            break;
      }
      free(rawAckToSend);
      return RETURN_FAILURE;
   }
   if (lengthAck != HEADER_SIZE)
   {
      ERROR("Couldn't encode the full acknowledgment to be sent");
      free(rawAckToSend);
      return RETURN_FAILURE;
   }

   //DEBUG
   fprintf(stderr, "Acknowledgment sent has seqnum : %d (window : %d)\n", pkt_get_seqnum(acknowledgmentToSend), pkt_get_window(acknowledgmentToSend));

   //--------------------- Send acknowledgment -----------------------
   if (sendto(sfd, rawAckToSend, lengthAck, 0, NULL, 0) != lengthAck)
   {
      ERROR("Couldn't write acknowledgment on socket");
      free(rawAckToSend);
      return RETURN_FAILURE;
   }

   free(rawAckToSend);

   //------------ Reset timer ---------------------
   gettimeofday(&timerAckSent, NULL);

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

   //printDataPktOutOfSequenceBuf();

   if (nbPktOutOfSequenceInBuf <= 0)
      return;

   //fprintf(stderr, "indexFirstOutOfSequencePkt : %d\n", indexFirstOutOfSequencePkt);

   while ((bufOutOfSequencePkt[indexFirstOutOfSequencePkt] != NULL) && (nbDataPktToWrite+1 < MAX_PACKETS_PREPARED))
   {
      lastSeqnumReceivedInOrder++;//equivalent to lastSeqnumReceivedInOrder = pkt_get_seqnum(bufOutOfSequencePkt[indexFirstOutOfSequencePkt]);

      //fprintf(stderr, "Taking pkt #%d out of out-of-sequence buffer\n", lastSeqnumReceivedInOrder);

      //put this packet in the buffer of in-sequence packets (to write in the output file)
      int nextIndex = (indexFirstDataPkt+nbDataPktToWrite)%MAX_PACKETS_PREPARED;
      dataPktInSequence[nextIndex] = bufOutOfSequencePkt[indexFirstOutOfSequencePkt];
      nbDataPktToWrite++;

      //remove packet from out-of-sequence buffer
      bufOutOfSequencePkt[indexFirstOutOfSequencePkt] = NULL;
      indexFirstOutOfSequencePkt++;
      indexFirstOutOfSequencePkt %= MAX_WINDOW_SIZE;
      nbPktOutOfSequenceInBuf--;
      if (nbPktOutOfSequenceInBuf < 0)//shouldn't be possible to happen
      {
         ERROR("Number of packets in the out-of-sequence buffer is negative");
      }
   }
}

bool stillSomethingToWrite()
{
   return (nbDataPktToWrite > 0);
}

void purgeBuffers()
{
   if (nbDataPktToWrite > 0)
   {
      WARNING("Purging not empty buffer of data packets to write on output file");
   }
   if (nbPktOutOfSequenceInBuf > 0)
   {
      WARNING("Purging not empty buffer of out-of-sequence data packets");
   }

   if (acknowledgmentToSend != NULL)
   {
      pkt_del(acknowledgmentToSend);
      acknowledgmentToSend = NULL;
   }

   int i;
   for (i=0; i<MAX_PACKETS_PREPARED; i++)
   {
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
   int i;
   for (i=0; i<MAX_WINDOW_SIZE; i++)
   {
      if (bufOutOfSequencePkt[i] != NULL)
         fprintf(stderr, "%d ", pkt_get_seqnum(bufOutOfSequencePkt[i]));
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
