#include "senderReadWriteLoop.h"

ERR_CODE senderReadWriteLoop(const int sfd, const int inputFile)
{
   if (sfd < 0)
   {
      ERROR("Invalid socket file descriptor");
      return RETURN_FAILURE;
   }
   if (inputFile < 0)
   {
      ERROR("Invalid input file descriptor");
      return RETURN_FAILURE;
   }

   uint8_t bufInput[MAX_PKT_SIZE], bufSocket[MAX_PKT_SIZE];
   int err;

   fd_set inputFdSet, socketFdSet, copyFdSet;
   FD_ZERO(&inputFdSet);
   FD_SET(inputFile, &inputFdSet);

   FD_ZERO(&socketFdSet);
   FD_SET(sfd, &socketFdSet);

   FD_ZERO(&copyFdSet);

   struct timeval tv, tvCopy;
   tv.tv_sec = 0;
   tv.tv_usec = 5000;

   int bytesRead;

   while (true)
   {
      //--------------- Read input file ------------------
      copyFdSet = inputFdSet;
      tvCopy = tv;

      err = select(inputFile+1, &copyFdSet, NULL, NULL, &tvCopy);
      if (err < 0)
      {
         perror("Couldn't read input file");
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {
         bytesRead = read(inputFile, bufInput, MAX_PKT_SIZE);
         if (bytesRead < 0)
         {
            perror("Couldn't read input file");
            return RETURN_FAILURE;
         }
         else if (bytesRead == 0)
         {
            //Received EOF
            return RETURN_SUCCESS;
         }
         else
         {
            //TODO
         }
      }

      //---------------- Write socket ----------------------
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

      }

      //---------------- Read socket (ack) ------------------------
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
            perror("Couldn't receive data from socket");
            return RETURN_FAILURE;
         }
         else if (bytesRead == 0)
         {
            //Received EOF from socket (possible?)
         }
         else
         {
            //TODO
            senderInterpretDataReceived(bufSocket, bytesRead);
         }
      }
   }

   return RETURN_SUCCESS;
}
