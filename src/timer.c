#include "timer.h"

bool isTimerOver(struct timeval beginningTime)
{
   struct timeval now;
   gettimeofday(&now, NULL);

   return ((now.tv_sec-beginningTime.tv_sec)*1000000+(now.tv_usec-beginningTime.tv_usec) >= RETRANSMISSION_TIME);
}

long elapsedTime(struct timeval beginningTime)
{
   struct timeval now;
   gettimeofday(&now, NULL);

   return (now.tv_sec-beginningTime.tv_sec)*1000000+(now.tv_usec-beginningTime.tv_usec);
}
