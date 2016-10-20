#include "senderManagePackets.h"

pkt_t* bufAckPkt[MAX_WINDOW_SIZE];//will contain pointers to acknowledgment packets
int nbEmptySpacesInBufAckPkt = MAX_WINDOW_SIZE;
uint8_t currentReceiverWindowSize = 1;//last window size received from receiver acknowledgments

pkt_t* bufPktToSend[MAX_PACKETS_PREPARED];//will contain the packets to send
int nbPktToSend = 0;//number of packets in the buffer of packets to send

uint8_t currentSeqnum = 0;//seqnum of the next pkt to create
uint8_t lastSeqnumAcknowledged;
bool alreadyReceivedAck = false;//true if at least one ack has been received since the beginning of the program

ERR_CODE senderInterpretDataReceived(const uint8_t* data, uint16_t length)
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
         //TODO
      }
   }

   return RETURN_SUCCESS;
}

pkt_t* createDataPkt(const uint8_t* payload, uint16_t length)
{
   if (payload == NULL || length == 0)
   {
      ERROR("tried to create a data packet with no payload");
      return NULL;
   }

   pkt_t pkt = pkt_new();
   if (pkt == NULL)
   {
      ERROR("Couldn't allocate a new data packet");
      return NULL;
   }

   //TODO test return values
   pkt_set_type(&pkt, PTYPE_DATA);
   pkt_set_window(&pkt, (uint8_t) nbEmptySpacesInBufAckPkt);
   pkt_set_seqnum(&pkt, currentSeqnum);
   pkt_set_length(&pkt, length);
   //TODO pkt_set_timestamp()
   pkt_set_payload(&pkt, payload, length);//crc is computed and put in pkt

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

   bufPktToSend[nbPktToSend] = dataPkt;
   nbPktToSend++;
   return RETURN_SUCCESS;
}
