#include "senderManagePackets.h"

uint8_t currentReceiverWindowSize = 1;//last window size received from receiver acknowledgments

pkt_t* bufPktToSend[MAX_PACKETS_PREPARED] = {NULL};//will contain the packets to send /!\ FIFO
int firstBufIndex = 0, nbPktToSend = 0;//index of the first pkt in the FIFO buffer - number of packets in the buffer of packets to send

uint8_t currentSeqnum = 0;//seqnum of the next pkt to create
uint8_t nextSeqnumToBeAck;//seqnum in the last ack (== seqnum next data packet to send)

pkt_t* createDataPkt(const uint8_t* payload, uint16_t length)
{
   if (payload == NULL || length == 0)
   {
      ERROR("tried to create a data packet with no payload");
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
   //TODO pkt_set_timestamp()
   pkt_set_payload(pkt, payload, length);//crc is computed and put in pkt

   return pkt;
}

ERR_CODE putNewPktInBufferToSend(pkt_t* dataPkt)
{
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

   int nextIndex = (firstBufIndex+nbPktToSend)%MAX_PACKETS_PREPARED;
   bufPktToSend[nextIndex] = dataPkt;
   nbPktToSend++;
   return RETURN_SUCCESS;
}

ERR_CODE sendDataPktFromBuffer(const int sfd)
{
   if (sfd < 0)
   {
      ERROR("Invalid socket file descriptor");
      return RETURN_FAILURE;
   }

   if (nbPktToSend == 0)//nothing to send in the FIFO buffer
      return RETURN_SUCCESS;

   //---- Create raw data buf --------
   uint8_t* tmpBufRawPkt = malloc(MAX_PKT_SIZE);
   size_t lengthTmpBuf = MAX_PKT_SIZE;
   //TODO test return value
   pkt_encode(bufPktToSend[firstBufIndex], tmpBufRawPkt, &lengthTmpBuf);

   //---- Send packet on socket ------
   if (sendto(sfd, tmpBufRawPkt, lengthTmpBuf, 0, NULL, 0) != lengthTmpBuf)
   {
      perror("Couldn't write a data packet on the socket");
      return RETURN_FAILURE;
   }

   free(tmpBufRawPkt);

   return RETURN_SUCCESS;
}

ERR_CODE receiveAck(const uint8_t* data, uint16_t length)
{
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

      //TODO ask packet to be sent again?
   }
   else//Valid pkt
   {
      if (pkt_get_type(&pktReceived) == PTYPE_DATA)
      {
         ERROR("Sender received a data packet");
      }
      else//Valid acknowledment pkt
      {
         uint8_t seqnum = pkt_get_seqnum(&pktReceived);
         removeDataPktFromBuffer(seqnum);
         nextSeqnumToBeAck = seqnum;
      }
   }

   return RETURN_SUCCESS;
}

void removeDataPktFromBuffer(uint8_t minSeqnumToKeep)
{
   //This might not work if two acknowledgments are received in wrong order
   while (nbPktToSend > 0 && pkt_get_seqnum(bufPktToSend[firstBufIndex]) != minSeqnumToKeep)
   {
      //removing packet at index firstBufIndex if it isn't the next packet to send (i.e. it has already been sent and received)
      pkt_del(bufPktToSend[firstBufIndex]);
      bufPktToSend[firstBufIndex] = NULL;
      firstBufIndex++;
      firstBufIndex %= MAX_PACKETS_PREPARED;
      nbPktToSend--;
   }
}
