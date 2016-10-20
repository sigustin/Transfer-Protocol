#include "receiverReadWriteLoop.h"

ERR_CODE readWriteLoopReceiver(const int sfd, const int outputFile)
{
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

   uint8_t bufSocket[MAX_PKT_SIZE], bufOutput[MAX_PKT_SIZE];
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
      //------------------- Read socket ----------------------
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
         bytesRead = recvfrom(sfd, bufSocket, MAX_PKT_SIZE, 0, NULL, NULL);
         if (bytesRead < 0)
         {
            perror("Couldn't read from socket");
            return RETURN_FAILURE;
         }
         else if (bytesRead == 0)
         {
            //Received EOF from socket
         }
         else
         {
            //TODO
            receiverInterpretDataReceived(bufSocket, bytesRead);
         }
      }

      //------------------- Write output file -------------------------
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
         //TODO
      }

      //---------------------- Write socket (ack) ---------------------
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
         //TODO
      }
   }

   return RETURN_SUCCESS;
}