#ifndef _DEFINES_H_
#define _DEFINES_H_

#define DEBUG(string);        fprintf(stderr, "DEBUG :\t%s\n", string);
#define DEBUG_FINE(string);   fprintf(stderr, "DEBUG :\t%s\n", string);
#define DEBUG_FINEST(string); //fprintf(stderr, "DEBUG :\t%s\n", string);

#define ERROR(string);	fprintf(stderr, "ERROR :\t%s\n", string);

#define MAX_PAYLOAD_SIZE   512
#define HEADER_SIZE        12
#define MAX_PKT_SIZE       MAX_PAYLOAD_SIZE+HEADER_SIZE

#define MAX_PACKETS_PREPARED  1024

#define ERR_CODE        int
#define RETURN_SUCCESS  0
#define RETURN_FAILURE  -1

#endif //_DEFINES_H_
