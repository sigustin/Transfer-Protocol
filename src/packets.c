#include "packets.h"

uint16_t getUInt16(const uint8_t* buf)
{
	//answer is written in native byte-order (thanks to bit shifting)
	uint16_t answer = buf[0] << 8;
	answer |= buf[1];
	return answer;
}

void putUInt16(uint16_t nb, uint8_t* buf)
{
	//the buf will be in the network byte-order
	buf[0] = (nb & 0xFF00) >> 8;
	buf[1] = (nb & 0x00FF);
}

uint32_t getUInt32(const uint8_t* buf)
{
	//answer is written in native byte-order (thanks to bit shifting)
	uint32_t answer = buf[0] << 24;
	answer |= buf[1] << 16;
	answer |= buf[2] << 8;
	answer |= buf[3];
	return answer;
}

void putUInt32(uint32_t nb, uint8_t* buf)
{
	//the buf will be in the network byte-order
	buf[0] = (nb & 0xFF000000) >> 24;
	buf[1] = (nb & 0x00FF0000) >> 16;
	buf[2] = (nb & 0x0000FF00) >> 8;
	buf[3] = (nb & 0x000000FF);
}

pkt_t* pkt_new()
{
	return calloc(1, sizeof(pkt_t));
}

void pkt_del(pkt_t *pkt)
{
   if (pkt->payload != NULL)
	 	free(pkt->payload);
	free(pkt);
}

pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{
	//Check for the consistency of the data
	if (data == NULL)
		return E_UNCONSISTENT;//should be spelled "inconsistent"
	if (len < 12)
		return E_NOHEADER;
	if (len-12 > MAX_PAYLOAD_SIZE)
		return E_LENGTH;

	const uint8_t* dataCast = (uint8_t*) data;

	//Extraction of the fields from array data and check for their consistency
	uint8_t type = dataCast[0] >> 5;
	if (type != PTYPE_DATA && type != PTYPE_ACK)
		return E_TYPE;

	uint8_t window = (dataCast[0] & 0x1F);

	uint8_t seqnum = dataCast[1];

	uint16_t payloadLength = getUInt16(&dataCast[2]);
	if (payloadLength != len-12)
		return E_LENGTH;

	uint32_t timestamp = getUInt32(&dataCast[4]);

	uint8_t* payload = malloc(payloadLength);
	int i;
	for (i=0; i<payloadLength; i++)
		payload[i] = dataCast[i+8];

	uint32_t crc = getUInt32(&dataCast[len-4]);

	//Computation of the CRC32 of the header and payload (using zlib's crc32 function)
	uint32_t computedCRC = crc32(0L, dataCast, payloadLength+8);
	if (computedCRC != crc)
		return E_CRC;

	//Set up of the structure pkt
	if (pkt == NULL)
		pkt = pkt_new();
	pkt->type = type;
	pkt->window = window;
	pkt->sequenceNb = seqnum;
	pkt->payloadLength = payloadLength;
	pkt->timestamp = ntohl(timestamp);
	pkt->payload = payload;
	pkt->crc = crc;

	return PKT_OK;
}

pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len)
{
	//Check for consistency of parameters
	if (pkt == NULL)
		return E_UNCONSISTENT;
	if (len == NULL || *len < 12)
		return E_NOMEM;

	//Check for length of buffer
	if (*len < (size_t) pkt->payloadLength+12)
		return E_NOMEM;

	uint8_t* bufCast = (uint8_t*) buf;

	//Fill up of the buffer
	bufCast[0] = (pkt->type << 5) | pkt->window;
	bufCast[1] = pkt->sequenceNb;
	putUInt16(pkt->payloadLength, &bufCast[2]);
	putUInt32(htonl(pkt->timestamp), &bufCast[4]);//doesn't pass inginious tests without changing the byte-order

	int i;
	for (i=0; i<pkt->payloadLength; i++)
		bufCast[i+8] = pkt->payload[i];

	//Computation of the CRC32 of the header and payload (using zlib's crc32 function)
	uint32_t crc = crc32(0L, bufCast, pkt->payloadLength+8);
	putUInt32(crc, &bufCast[pkt->payloadLength+8]);

	*len = pkt->payloadLength+12;

	return PKT_OK;
}

ptypes_t pkt_get_type(const pkt_t* pkt)
{
	if (pkt == NULL)
		return 0;
	return pkt->type;
}

uint8_t  pkt_get_window(const pkt_t* pkt)
{
	if (pkt == NULL)
		return 0;
	return pkt->window;
}

uint8_t  pkt_get_seqnum(const pkt_t* pkt)
{
	if (pkt == NULL)
		return 0;
	return pkt->sequenceNb;
}

uint16_t pkt_get_length(const pkt_t* pkt)
{
	if (pkt == NULL)
		return 0;
	return pkt->payloadLength;
}

uint32_t pkt_get_timestamp (const pkt_t* pkt)
{
	if (pkt == NULL)
		return 0;
	return pkt->timestamp;
}

uint32_t pkt_get_crc (const pkt_t* pkt)
{
	if (pkt == NULL)
		return 0;
	return pkt->crc;
}

const char* pkt_get_payload(const pkt_t* pkt)
{
	if (pkt == NULL)
		return NULL;
	return (const char*) pkt->payload;
}


pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type)
{
	if (pkt == NULL)
		return E_UNCONSISTENT;
	if (type != PTYPE_DATA && type != PTYPE_ACK)
		return E_TYPE;

	pkt->type = type;
	return PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
	if (pkt == NULL)
		return E_UNCONSISTENT;
	if (window > MAX_WINDOW_SIZE)
		return E_WINDOW;

	pkt->window = window;
	return PKT_OK;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
	if (pkt == NULL)
		return E_UNCONSISTENT;

	pkt->sequenceNb = seqnum;
	return PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
	if (pkt == NULL)
		return E_UNCONSISTENT;
	if (length > MAX_PAYLOAD_SIZE)
		return E_LENGTH;

	pkt->payloadLength = length;
	return PKT_OK;
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
	if (pkt == NULL)
		return E_UNCONSISTENT;

	pkt->timestamp = timestamp;
	return PKT_OK;
}

pkt_status_code pkt_set_crc(pkt_t *pkt, const uint32_t crc)
{
	if (pkt == NULL)
		return E_UNCONSISTENT;

	pkt->crc = crc;
	return PKT_OK;
}

pkt_status_code pkt_set_payload(pkt_t *pkt,
							    const char *data,
								const uint16_t length)
{
	if (pkt == NULL)
		return E_UNCONSISTENT;
	if (data == NULL || length == 0)
		return E_LENGTH;

	uint8_t* dataCast = (uint8_t*) data;

	if (pkt->payload != NULL)
		free(pkt->payload);
	pkt->payload = malloc(length);

	int i;
	for (i=0; i<length; i++)
		pkt->payload[i] = dataCast[i];

	pkt->payloadLength = length;

	//Computation of the CRC32
	uint8_t* headerAndPayload = malloc(length+8);
	headerAndPayload[0] = (pkt->type << 5) | pkt->window;
	headerAndPayload[1] = pkt->sequenceNb;
	putUInt16(pkt->payloadLength, &headerAndPayload[2]);
	putUInt32(pkt->timestamp, &headerAndPayload[4]);
	for (i=0; i<length; i++)
		headerAndPayload[i+8] = dataCast[i];

	uint32_t crc = crc32(0L, headerAndPayload, length+8);
	pkt->crc = crc;

	return PKT_OK;
}
