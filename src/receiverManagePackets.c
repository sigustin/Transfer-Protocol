#include "receiverManagePackets.h"

extern pkt_t* acknowledmentsToSend[MAX_PACKETS_PREPARED];//buffer containing the acknowledments to be send by receiverReadWriteLoop
extern int indexFirstAckToSend, nbAckToSend;

extern pkt_t* dataPktInSequence[MAX_PACKETS_PREPARED];//will contain the packets received in sequence and that have to be written on the output file
extern int indexFirstDataPkt, nbDataPktToWrite;

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
         if (lastSeqnumReceivedInOrder+1 == seqnum)//packet is in sequence
         {
            lastSeqnumReceivedInOrder++;

            dataPktInSequence[(indexFirstDataPkt+nbDataPktToWrite)%MAX_PACKETS_PREPARED] = &pktReceived;
            nbDataPktToWrite++;
            //TODO check packet is not the last packet
         }
         else//packet is out-of-sequence
         {
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

         acknowledmentsToSend[(indexFirstAckToSend+nbAckToSend)%MAX_PACKETS_PREPARED] = newAck;
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
