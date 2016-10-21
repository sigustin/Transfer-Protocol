#include "receiverManagePackets.h"

uint8_t lastSeqnumReceivedInOrder = 0;
uint32_t lastTimestampReceived = 0;

pkt_t* bufPktReceived[MAX_WINDOW_SIZE] = {NULL};//Contains out-of-sequence received packets
int firstPktBufIndex = 0, nbPktReceivedInBuf = 0;//window size = MAX_WINDOW_SIZE-nbPktReceivedInBuf

pkt_t* bufAckPrepared[MAX_PACKETS_PREPARED] = {NULL};//Contains ack ready to be sent (in order)
int firstAckBufIndex = 0, nbAckPreaparedInBuf = 0;

//TODO timers

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
         if (lastSeqnumReceivedInOrder+1 == seqnum)//packet is in sequence
         {
            lastSeqnumReceivedInOrder++;

            const uint8_t* dataPayload = pkt_get_payload(&pktReceived);
            //TODO put in buffer of things to write in output file
            //TODO check packet is not the last packet
         }
         else//packet is out-of-sequence
         {
            //---------- Check if seqnum is in the window and put it in the buffer if it is ------------------
            //TODO check return value
            putReceivedPktInBuf(&pktReceived);
         }

         //--------- Prepare ack and put it in buffer to send ----------------------
         pkt_t* newAck = createNewAck();
         if (newAck == NULL)
         {
            ERROR("Couldn't create new acknowledgment");
            return RETURN_FAILURE;
         }

         //TODO check return value
         putAckInBuf(newAck);
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

ERR_CODE putAckInBuf(pkt_t* ack)
{
   if (ack == NULL)
   {
      ERROR("Tried to put no acknowledment in buffer");
      return RETURN_FAILURE;
   }

   int nextIndex = (firstAckBufIndex+nbAckPreaparedInBuf)%MAX_PACKETS_PREPARED;
   bufAckPrepared[nextIndex] = ack;
   nbAckPreaparedInBuf++;
   //TODO zero out timer

   return RETURN_SUCCESS;
}

ERR_CODE putReceivedPktInBuf(pkt_t* dataPkt)
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
      if (bufPktReceived[index] != NULL)//not supposed to happen
      {
         ERROR("There was a problem with the buffer containing packets out-of-sequence.\nTried to overwrite data")
         return RETURN_FAILURE;
      }

      bufPktReceived[index] = dataPkt;
   }

   return RETURN_SUCCESS;
}
