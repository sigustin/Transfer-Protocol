#include "receiverManagePackets.h"

//TODO purge buffers when an error terminates the program

extern bool lastPktReceived;

pkt_t* acknowledgmentsToSend[MAX_PACKETS_PREPARED] = {NULL};//buffer containing the acknowledments to be send by receiverReadWriteLoop
int indexFirstAckToSend = 0, nbAckToSend = 0;
struct timeval timerAckSent;

pkt_t* dataPktInSequence[MAX_PACKETS_PREPARED] = {NULL};//will contain the packets received in sequence and that have to be written on the output file
int indexFirstDataPkt = 0, nbDataPktToWrite = 0;

uint8_t lastSeqnumReceivedInOrder = 0;
uint32_t lastTimestampReceived = 0;

pkt_t* bufOutOfSequencePkt[MAX_WINDOW_SIZE] = {NULL};//Contains out-of-sequence received packets
int firstPktBufIndex = 0, nbPktReceivedInBuf = 0;//window size = MAX_WINDOW_SIZE-nbPktReceivedInBuf

ERR_CODE receiveDataPacket(const uint8_t* data, int length)
{
   if (length <= 0)
   {
      ERROR("Tried to interpret data buffer with no length");
      return RETURN_FAILURE;
   }

   //------------ Decode data --------------------
   pkt_t pktReceived;
   pkt_status_code errCode;
   errCode = pkt_decode(data, length, &pktReceived);
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
      pkt_del(&pktReceived);
      return RETURN_FAILURE;
   }
   else//Valid pkt
   {
      if (pkt_get_type(&pktReceived) == PTYPE_ACK)
      {
         ERROR("Received acknowledgment from sender");
      }
      else if (pkt_get_length(&pktReceived) > MAX_PAYLOAD_SIZE)//ignore packet
      {}
      else//Valid data pkt
      {
         DEBUG("Interpreting valid data packet");

         lastTimestampReceived = pkt_get_timestamp(&pktReceived);

         //------ Check if packet is in sequence ------------------
         uint8_t seqnum = pkt_get_seqnum(&pktReceived);
         fprintf(stderr, "Valid packet has seqnum : %d\n", seqnum);
         if (lastSeqnumReceivedInOrder+1 == seqnum)//packet is in sequence
         {
            DEBUG_FINE("Data packet in sequence");

            lastSeqnumReceivedInOrder++;

            if (pkt_get_length(&pktReceived) == 0)
               lastPktReceived = true;
            else
            {
               dataPktInSequence[(indexFirstDataPkt+nbDataPktToWrite)%MAX_PACKETS_PREPARED] = &pktReceived;
               nbDataPktToWrite++;
            }
         }
         else//packet is out-of-sequence
         {
            DEBUG_FINE("Data packet out-of-sequence");
            //---------- Check if seqnum is in the window and put it in the buffer if it is ------------------
            //TODO check return value
            putOutOfSequencePktInBuf(&pktReceived);
         }

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
   pkt_set_window(ack, (uint8_t) MAX_WINDOW_SIZE-nbPktReceivedInBuf);
   pkt_set_seqnum(ack, lastSeqnumReceivedInOrder+1);//the seqnum of the next packet that the sender must send
   pkt_set_length(ack, 0);
   pkt_set_timestamp(ack, lastTimestampReceived);
   pkt_set_payload(ack, NULL, 0);//sets crc

   return ack;
}

ERR_CODE putOutOfSequencePktInBuf(pkt_t* dataPkt)
{
   if (dataPkt == NULL)
   {
      ERROR("Tried to put no data packet in buffer of received packets");
      return RETURN_FAILURE;
   }
   if (nbPktReceivedInBuf == MAX_WINDOW_SIZE)//no place in buffer
   {
      ERROR("Tried to put data packet in full buffer");
      return RETURN_FAILURE;
   }

   //------ Compute distance between the first seqnum that should be received to make the window move -----
   uint8_t firstSeqnumInBuf = lastSeqnumReceivedInOrder+1, seqnum = pkt_get_seqnum(dataPkt);
   int distanceSeqnumToFirstWindowSeqnum;
   if (firstSeqnumInBuf > seqnum)
      distanceSeqnumToFirstWindowSeqnum = seqnum - firstSeqnumInBuf;
   else
      distanceSeqnumToFirstWindowSeqnum = (NB_DIFFERENT_SEQNUM - firstSeqnumInBuf) + seqnum;

   //----- Put packet at the right place in the buffer -------
   if (distanceSeqnumToFirstWindowSeqnum > MAX_WINDOW_SIZE)//out of buffer
   {}//discard
   else
   {
      int index = (firstPktBufIndex + distanceSeqnumToFirstWindowSeqnum) % MAX_WINDOW_SIZE;
      if (bufOutOfSequencePkt[index] != NULL)//not supposed to happen
      {
         ERROR("There was a problem with the buffer containing packets out-of-sequence.\nTried to overwrite data")
         return RETURN_FAILURE;
      }

      bufOutOfSequencePkt[index] = dataPkt;
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
      //send all acknowledgments and remove them except the last one
      while (nbAckToSend >= 1)
      {
         DEBUG_FINE("Send acknowledgment");

         if (sendFirstAckFromBuffer(sfd) != RETURN_SUCCESS)
            return RETURN_FAILURE;

         //reset timer
         gettimeofday(&timerAckSent, NULL);

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

   if (lastPktReceived)//last acknowledgment has been sent at least once
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

   if (sendto(sfd, rawAckToSend, lengthAck, 0, NULL, 0) != lengthAck)
   {
      ERROR("Couldn't write acknowledment on socket");
      free(rawAckToSend);
      return RETURN_FAILURE;
   }

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

      if (write(fd, pkt_get_payload(dataPktInSequence[indexFirstDataPkt]), pkt_get_length(dataPktInSequence[indexFirstDataPkt])) == -1)
      {
         perror("Couldn't write payload in output file");
         return RETURN_FAILURE;
      }

      pkt_del(dataPktInSequence[indexFirstDataPkt]);
      dataPktInSequence[indexFirstDataPkt] = NULL;

      indexFirstDataPkt++;
      indexFirstDataPkt %= MAX_PACKETS_PREPARED;
      nbDataPktToWrite--;
   }

   return RETURN_SUCCESS;
}

bool stillSomethingToWrite()
{
   return (nbDataPktToWrite > 0) || (nbAckToSend > 0);
}
