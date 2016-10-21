#include "receiverReadWriteLoop.h"

bool lastPktReceived = false;

ERR_CODE receiverReadWriteLoop(const int sfd, const int outputFile)
{
   DEBUG("readWriteLoopReceiver");

   if (sfd < 0)
   {
      ERROR("Invalid socket file descriptor");
      return RETURN_FAILURE;
   }
   if (outputFile < 0)
   {
      ERROR("Invalid output file descriptor");
      return RETURN_FAILURE;
   }

   uint8_t bufSocket[MAX_PKT_SIZE];
   int err;

   fd_set socketFdSet, outputFdSet, copyFdSet;
   FD_ZERO(&socketFdSet);
   FD_SET(sfd, &socketFdSet);

   FD_ZERO(&outputFdSet);
   FD_SET(outputFile, &outputFdSet);

   FD_ZERO(&copyFdSet);

   struct timeval tv, tvCopy;
   tv.tv_sec = 0;
   tv.tv_usec = 5000;

   int bytesRead;

   while (true)
   {
      if (!lastPktReceived)
      {
         //================== Read socket ====================
         copyFdSet = socketFdSet;
         tvCopy = tv;

         err = select(sfd+1, &copyFdSet, NULL, NULL, &tvCopy);
         if (err < 0)
         {
            perror("Couldn't read socket");
            return RETURN_FAILURE;
         }
         else if (err > 0)
         {
            DEBUG_FINE("Trying to read from socket");

            bytesRead = recvfrom(sfd, bufSocket, MAX_PKT_SIZE, 0, NULL, NULL);
            if (bytesRead < 0)
            {
               perror("Couldn't read from socket");
               return RETURN_FAILURE;
            }
            else if (bytesRead == 0)//Received EOF from socket (possible?)
            {}
            else
            {
               receiveDataPacket(bufSocket, bytesRead);
            }
         }
      }

      //=================== Write output file ========================
      copyFdSet = outputFdSet;
      tvCopy = tv;

      err = select(outputFile+1, NULL, &copyFdSet, NULL, &tvCopy);
      if (err < 0)
      {
         perror("Couldn't write to output file");
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {
         DEBUG_FINEST("Trying to write to output file (if there's something to write)");

         if (writePayloadInOutputFile(outputFile) != RETURN_SUCCESS)
         {
            ERROR("Couldn't write data in output file");
            return RETURN_FAILURE;
         }
      }

      //===================== Write socket (ack) ========================
      copyFdSet = socketFdSet;
      tvCopy = tv;

      err = select(sfd+1, NULL, &copyFdSet, NULL, &tvCopy);
      if (err < 0)
      {
         perror("Couldn't write on socket");
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {
         DEBUG_FINEST("Trying to write on socket (if there's an acknowledment to write)")

         if (sendAckFromBuffer(sfd) != RETURN_SUCCESS)
         {
            ERROR("Couldn't send acknowledgments on socket");
            return RETURN_FAILURE;
         }
      }

      if (lastPktReceived && !stillSomethingToWrite())
         break;
   }

   return RETURN_SUCCESS;
}
