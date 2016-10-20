#include "receiverManagePackets.h"

ERR_CODE receiverInterpretDataReceived(const uint8_t* data, int length)
{
   if (length <= 0)
   {
      ERROR("Tried to interpret data buffer with no length");
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

      //TODO ask to send the packet again
   }
   else//Valid pkt
   {
      if (pkt_get_type(&pktReceived) == PTYPE_ACK)
      {
         ERROR("Received acknowledment from sender");
      }
      else//Valid data pkt
      {
         //TODO
      }
   }

   return RETURN_SUCCESS;
}
