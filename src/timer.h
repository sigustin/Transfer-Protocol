#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>

#define RETRANSMISSION_TIME   2000000//microseconds

/*
 * @return :   true if more than @RETRANSMISSION_TIME microseconds have passed since @beginningTime
 *             false otherwise
 */
bool isTimerOver(struct timeval beginningTime);

/*
 * @return : the elapsed time since @beginningTime (in microseconds)
 */
long elapsedTime(struct timeval beginningTime);

/*
 * @return : tv in microseconds
 */
long getTimeInMicroseconds(struct timeval tv);

#endif //_TIMER_H_
