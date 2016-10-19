#include "readWriteLoop.h"

ERR_CODE readWriteLoopSender(const int sfd, const int inputFile)
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

   struct timeval tv;
   tv.tv_sec = 0;
   tv.tv_usec = 5000;

   while (true)
   {
      //--------------- Read input file ------------------
      copyFdSet = inputFdSet;

      err = select(inputFile+1, &copyFdSet, NULL, NULL, &tv);
      if (err < 0)
      {
         perror("Couldn't read input file");
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {

      }

      //---------------- Write socket ----------------------
      copyFdSet = socketFdSet;

      err = select(sfd+1, NULL, &copyFdSet, NULL, &tv);
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

      err = select(sfd+1, &copyFdSet, NULL, NULL, &tv);
      if (err < 0)
      {
         perror("Couldn't read socket");
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {

      }
   }

   return RETURN_SUCCESS;
}

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

   struct timeval tv;
   tv.tv_sec = 0;
   tv.tv_usec = 5000;

   while (true)
   {
      //------------------- Read socket ----------------------
      copyFdSet = socketFdSet;

      err = select(sfd+1, &copyFdSet, NULL, NULL, &tv);
      if (err < 0)
      {
         perror("Couldn't read socket");
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {

      }

      //------------------- Write output file -------------------------
      copyFdSet = outputFdSet;

      err = select(outputFile+1, NULL, &copyFdSet, NULL, &tv);
      if (err < 0)
      {
         perror("Couldn't write to output file");
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {

      }

      //---------------------- Write socket (ack) ---------------------
      copyFdSet = socketFdSet;

      err = select(sfd+1, NULL, &copyFdSet, NULL, &tv);
      if (err < 0)
      {
         perror("Couldn't write on socket");
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {

      }
   }

   return RETURN_SUCCESS;
}