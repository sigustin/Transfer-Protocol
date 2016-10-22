#include "senderManagePackets.h"

extern bool EOFPktSent, EOFPktAck;

uint8_t currentReceiverWindowSize = 1;//last window size received from receiver acknowledgments

pkt_t* bufPktToSend[MAX_PACKETS_PREPARED] = {NULL};//will contain the packets to send /!\ FIFO
int firstBufIndex = 0, nbPktToSend = 0;//index of the first pkt in the FIFO buffer - number of packets in the buffer of packets to send
struct timeval bufPktSentTimers[MAX_PACKETS_PREPARED];//will contain the time when the packet in the FIFO buffer at the same index was last sent

uint8_t currentSeqnum = 0;//seqnum of the next pkt to create

pkt_t* createDataPkt(const uint8_t* payload, uint16_t length)
{
   DEBUG_FINE("createDataPkt");

   if (length > MAX_PAYLOAD_SIZE)
   {
      ERROR("Tried to create a data packet with to great payload");
      return NULL;
   }

   pkt_t* pkt = pkt_new();
   if (pkt == NULL)
   {
      ERROR("Couldn't allocate a new data packet");
      return NULL;
   }

   //TODO test return values
   pkt_set_type(pkt, PTYPE_DATA);
   pkt_set_window(pkt, currentReceiverWindowSize);
   pkt_set_seqnum(pkt, currentSeqnum);
   currentSeqnum++;//255++ == 0 since it's a uint8_t
   pkt_set_length(pkt, length);
   pkt_set_timestamp(pkt, 0);//TODO
   pkt_set_payload(pkt, payload, length);//crc is computed and put in pkt

   return pkt;
}

ERR_CODE putNewPktInBufferToSend(pkt_t* dataPkt)
{
   DEBUG_FINE("putNewPktInBufferToSend");

   if (dataPkt == NULL)
   {
      ERROR("Tried to put empty packet in buffer of packets ready to be sent");
      return RETURN_FAILURE;
   }
   if (nbPktToSend >= MAX_PACKETS_PREPARED-1)
   {
      ERROR("Tried to prepare too much data packets (buffer full)");
      return RETURN_FAILURE;
   }

   //put pkt in buffer
   int nextIndex = (firstBufIndex+nbPktToSend)%MAX_PACKETS_PREPARED;
   bufPktToSend[nextIndex] = dataPkt;
   nbPktToSend++;
   //reset timer at the same index in timers buffer
   bufPktSentTimers[nextIndex].tv_sec = bufPktSentTimers[nextIndex].tv_usec = 0;

   //DEBUG
   printBuffer();

   return RETURN_SUCCESS;
}

ERR_CODE sendDataPktFromBuffer(const int sfd)
{
   DEBUG_FINEST("sendDataPktFromBuffer");

   if (sfd < 0)
   {
      ERROR("Invalid socket file descriptor");
      return RETURN_FAILURE;
   }

   if (nbPktToSend == 0)//nothing to send in the FIFO buffer
      return RETURN_SUCCESS;
   if (currentReceiverWindowSize == 0)//receiver cannot receive anything for the moment
      return RETURN_SUCCESS;

   /*
    * firstBufIndex is the index of the first packet in the sending window
    * packets from index firstBufIndex to firstBufIndex+currentReceiverWindowSize should be sent (if they haven't yet been sent or if their timer has ran out)
    */
   //TODO this will be a go-back-n protocol with this for loop
   int i;
   for (i=0; i<currentReceiverWindowSize && i<nbPktToSend; i++)
   {
      //---- Check timer (at the same index in bufPktSentTimers) is over before sending ----
      //rem : if pkt has not been sent yet, its sending time will be 0 => isTimerOver == true
      if (!isTimerOver(bufPktSentTimers[firstBufIndex+i]))
         continue;

      //------- Check if it is an EOF packet ---------------
      if (pkt_get_length(bufPktToSend[firstBufIndex+i]) == 0)
         EOFPktSent = true;

      //---- Create raw data buf from the i_th packet in the sending window --------
      uint8_t* tmpBufRawPkt = malloc(MAX_PKT_SIZE);
      size_t lengthTmpBuf = MAX_PKT_SIZE;

      //TODO test return value
      pkt_encode(bufPktToSend[firstBufIndex+i], tmpBufRawPkt, &lengthTmpBuf);

      //---- Send packet on socket ------
      fprintf(stderr, "Send data on the socket (pkt #%d)\n", pkt_get_seqnum(bufPktToSend[firstBufIndex+i]));
      if (sendto(sfd, tmpBufRawPkt, lengthTmpBuf, 0, NULL, 0) != lengthTmpBuf)
      {
         perror("Couldn't write a data packet on the socket");
         return RETURN_FAILURE;
      }
      free(tmpBufRawPkt);

      //---------- Reset timer ----------
      gettimeofday(&(bufPktSentTimers[firstBufIndex+i]), NULL);
   }

   return RETURN_SUCCESS;
}

ERR_CODE receiveAck(const uint8_t* data, uint16_t length)
{
   DEBUG_FINE("receiveAck");

   if (length == 0)
   {
      ERROR("Tried to interpret a data buffer with no length");
      return RETURN_FAILURE;
   }

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

      //TODO ask packet to be sent again or ignore packet?
      pkt_del(&pktReceived);
      return RETURN_FAILURE;
   }
   else//Valid pkt
   {
      if (pkt_get_type(&pktReceived) == PTYPE_DATA)
      {
         ERROR("Sender received a data packet");
      }
      else//Valid acknowledment pkt
      {
         DEBUG_FINE("Interpreting valid acknowledment packet");

         uint8_t seqnum = pkt_get_seqnum(&pktReceived);

         fprintf(stderr, "Acknowledgment has seqnum : %d\n", seqnum);

         removeDataPktFromBuffer(seqnum);

         currentReceiverWindowSize = pkt_get_window(&pktReceived);
      }
   }

   return RETURN_SUCCESS;
}

void removeDataPktFromBuffer(uint8_t minSeqnumToKeep)
{
   DEBUG_FINE("removeDataPktFromBuffer");

   //This might not work if two acknowledgments are received in wrong order
   while (nbPktToSend > 0 && pkt_get_seqnum(bufPktToSend[firstBufIndex]) != minSeqnumToKeep)
   {
      fprintf(stderr, "Removing packet #%d from buffer\n", pkt_get_seqnum(bufPktToSend[firstBufIndex]));

      if (pkt_get_length(bufPktToSend[firstBufIndex]) == 0)
         EOFPktAck = true;

      //removing packet at index firstBufIndex if it isn't the next packet to send (i.e. it has already been sent and received)
      pkt_del(bufPktToSend[firstBufIndex]);
      bufPktToSend[firstBufIndex] = NULL;

      //reset timer
      bufPktSentTimers[firstBufIndex].tv_sec = bufPktSentTimers[firstBufIndex].tv_usec = 0;

      //move sliding window
      firstBufIndex++;
      firstBufIndex %= MAX_PACKETS_PREPARED;
      nbPktToSend--;
   }
}

void purgeBuffers()
{
   if (nbPktToSend > 0)
   {
      WARNING("Purging not empty buffer of packets to send");
   }

   int i;
   for (i=0; i<MAX_PACKETS_PREPARED; i++)
   {
      if (bufPktToSend[i] != NULL)
         pkt_del(bufPktToSend[i]);
   }
}

void printBuffer()
{
   if (nbPktToSend == 0)
   {
      DEBUG_FINEST("Buffer of packets to send is empty");
      return;
   }

   fprintf(stderr, "Content of the buffer of packets to send\n");
   int i;
   for (i=0; i<nbPktToSend; i++)
   {
      fprintf(stderr, "\tPacket to send #%d (%d)\n", pkt_get_seqnum(bufPktToSend[firstBufIndex+i]), i);
      fprintf(stderr, "\t\ttype : %d\n", pkt_get_type(bufPktToSend[firstBufIndex+i]));
      fprintf(stderr, "\t\twindow : %d\n", pkt_get_window(bufPktToSend[firstBufIndex+i]));
      fprintf(stderr, "\t\tseqnum : %d\n", pkt_get_seqnum(bufPktToSend[firstBufIndex+i]));
      fprintf(stderr, "\t\tlength : %d\n", pkt_get_length(bufPktToSend[firstBufIndex+i]));
      fprintf(stderr, "\t\ttimestamp : %d\n", pkt_get_timestamp(bufPktToSend[firstBufIndex+i]));
      fprintf(stderr, "\t\tcrc : %d\n", pkt_get_crc(bufPktToSend[firstBufIndex+i]));
   }
}
