#include "senderReadWriteLoop.h"

bool lastPktSent = false, lastPktAck = false;

ERR_CODE senderReadWriteLoop(const int sfd, const int inputFile)
{
   DEBUG("senderReadWriteLoop");

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

   while (!lastPktAck)
   {
      //================= Read input file (make new data packet) =========================
      copyFdSet = inputFdSet;
      tvCopy = tv;

      err = select(inputFile+1, &copyFdSet, NULL, NULL, &tvCopy);
      if (err < 0)
      {
         perror("Couldn't read input file");
         purgeBuffers();
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {
         DEBUG("Trying to read from input file");
         bytesRead = read(inputFile, bufInput, MAX_PAYLOAD_SIZE);
         if (bytesRead < 0)
         {
            perror("Couldn't read input file");
            purgeBuffers();
            return RETURN_FAILURE;
         }
         else if (bytesRead == 0)
         {
            if (!lastPktSent)
            {
               //Received EOF
               DEBUG("Read EOF");//BUG sigseg
               pkt_t* EOFPkt = createDataPkt(NULL, 0);
               if (EOFPkt == NULL)
               {
                  ERROR("Couldn't create EOF packet");
               }
               else
                  putNewPktInBufferToSend(EOFPkt);
            }
         }
         else
         {
            //------------------- Create new data packet ----------------------------
            pkt_t* newDataPkt = createDataPkt(bufInput, bytesRead);
            if (newDataPkt == NULL)
            {
               ERROR("Couldn't create new data packet");
            }
            else
            {
               //----------------- Put data packet in buffer to be sent ----------------------
               putNewPktInBufferToSend(newDataPkt);//TODO test return value
            }
         }
      }

      //================ Write socket (write from packet buffer to socket) =========================
      copyFdSet = socketFdSet;
      tvCopy = tv;

      err = select(sfd+1, NULL, &copyFdSet, NULL, &tvCopy);
      if (err < 0)
      {
         perror("Couldn't write on socket");
         purgeBuffers();
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {
         //------------ Try to send packet (if there's something to send) -------------------------
         DEBUG_FINEST("Trying to send data on the socket (if there's something to send)");
         if (sendDataPktFromBuffer(sfd) != RETURN_SUCCESS)
         {
            ERROR("Couldn't write data packet on socket");
            purgeBuffers();
            return RETURN_FAILURE;
         }
      }

      //=================== Read socket (ack) ========================================
      copyFdSet = socketFdSet;
      tvCopy = tv;

      err = select(sfd+1, &copyFdSet, NULL, NULL, &tvCopy);
      if (err < 0)
      {
         perror("Couldn't read socket");
         purgeBuffers();
         return RETURN_FAILURE;
      }
      else if (err > 0)
      {
         DEBUG("Trying to receive data from socket");
         bytesRead = recvfrom(sfd, bufSocket, MAX_PKT_SIZE, 0, NULL, NULL);
         if (bytesRead < 0)
         {
            perror("Couldn't receive data from socket");//Connection refused if no receiver is running
            purgeBuffers();
            return RETURN_FAILURE;
         }
         else if (bytesRead == 0)
         {}//Received EOF from socket (possible?)
         else
         {
            //Decode data, check packet and update FIFO buffer of packets to send
            if(receiveAck(bufSocket, bytesRead) != RETURN_SUCCESS)
            {
               ERROR("Couldn't read acknowledment from socket");
            }
         }
      }
   }

   DEBUG("Exit : last packet acknowledged");
   purgeBuffers();

   return RETURN_SUCCESS;
}
