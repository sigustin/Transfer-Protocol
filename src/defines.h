#ifndef _DEFINES_H_
#define _DEFINES_H_

#define ERROR(string);	fprintf(stderr, "ERROR :\t%s\n", string);

#define MAX_PAYLOAD_SIZE   512
#define MAX_PKT_SIZE       MAX_PAYLOAD_SIZE+12

#define ERR_CODE        int
#define RETURN_SUCCESS  0
#define RETURN_FAILURE  -1

#endif //_DEFINES_H_
