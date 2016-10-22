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

pkt_status_code pkt_decode(const uint8_t *data, const size_t len, pkt_t *pkt)
{
	//Check for the consistency of the data
	if (data == NULL)
		return E_INCONSISTENT;
	if (len < HEADER_SIZE)
		return E_NOHEADER;
	if (len-HEADER_SIZE > MAX_PAYLOAD_SIZE)
		return E_LENGTH;

	//Extraction of the fields from array data and check for their consistency
	uint8_t type = data[0] >> 5;
	if (type != PTYPE_DATA && type != PTYPE_ACK)
		return E_TYPE;

	uint8_t window = (data[0] & 0x1F);

	uint8_t seqnum = data[1];

	uint16_t payloadLength = getUInt16(&data[2]);
	if (payloadLength != len-HEADER_SIZE)
		return E_LENGTH;

	uint32_t timestamp = getUInt32(&data[4]);

	uint8_t* payload = malloc(payloadLength);
	int i;
	for (i=0; i<payloadLength; i++)
		payload[i] = data[i+8];

	uint32_t crc = getUInt32(&data[len-4]);

	//Computation of the CRC32 of the header and payload (using zlib's crc32 function)
	uint32_t computedCRC = crc32(0L, data, payloadLength+8);
	if (computedCRC != crc)
		return E_CRC;

	//Set up of the structure pkt
	if (pkt == NULL)
		pkt = pkt_new();
	pkt->type = type;
	pkt->window = window;
	pkt->sequenceNb = seqnum;
	pkt->payloadLength = payloadLength;
	pkt->timestamp = timestamp;
	pkt->payload = payload;
	pkt->crc = crc;

	return PKT_OK;
}

pkt_status_code pkt_encode(const pkt_t* pkt, uint8_t *buf, size_t *len)
{
	//Check for consistency of parameters
	if (pkt == NULL)
		return E_INCONSISTENT;
	if (len == NULL || *len < HEADER_SIZE)
		return E_NOMEM;

	//Check for length of buffer
	if (*len < (size_t) pkt->payloadLength+HEADER_SIZE)
		return E_NOMEM;

	//Fill up of the buffer
	buf[0] = (pkt->type << 5) | pkt->window;
	buf[1] = pkt->sequenceNb;
	putUInt16(pkt->payloadLength, &buf[2]);
	putUInt32(pkt->timestamp, &buf[4]);//doesn't pass inginious tests without changing the byte-order
	
	int i;
	for (i=0; i<pkt->payloadLength; i++)
		buf[i+8] = pkt->payload[i];

	//Computation of the CRC32 of the header and payload (using zlib's crc32 function)
	uint32_t crc = crc32(0L, buf, pkt->payloadLength+8);
	putUInt32(crc, &buf[pkt->payloadLength+8]);

	*len = pkt->payloadLength+HEADER_SIZE;

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

const uint8_t* pkt_get_payload(const pkt_t* pkt)
{
	if (pkt == NULL)
		return NULL;
	return (const uint8_t*) pkt->payload;
}


pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type)
{
	if (pkt == NULL)
		return E_INCONSISTENT;
	if (type != PTYPE_DATA && type != PTYPE_ACK)
		return E_TYPE;

	pkt->type = type;
	return PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
	if (pkt == NULL)
		return E_INCONSISTENT;
	if (window > MAX_WINDOW_SIZE)
		return E_WINDOW;

	pkt->window = window;
	return PKT_OK;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
	if (pkt == NULL)
		return E_INCONSISTENT;

	pkt->sequenceNb = seqnum;
	return PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
	if (pkt == NULL)
		return E_INCONSISTENT;
	if (length > MAX_PAYLOAD_SIZE)
		return E_LENGTH;

	pkt->payloadLength = length;
	return PKT_OK;
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
	if (pkt == NULL)
		return E_INCONSISTENT;

	pkt->timestamp = timestamp;
	return PKT_OK;
}

pkt_status_code pkt_set_crc(pkt_t *pkt, const uint32_t crc)
{
	if (pkt == NULL)
		return E_INCONSISTENT;

	pkt->crc = crc;
	return PKT_OK;
}

pkt_status_code pkt_set_payload(pkt_t *pkt, const uint8_t *data, const uint16_t length)
{
	if (pkt == NULL)
		return E_INCONSISTENT;
	if (data == NULL || length == 0)
	{
		uint8_t* headerRaw = malloc(8);
		headerRaw[0] = (pkt->type << 5) | pkt->window;
		headerRaw[1] = pkt->sequenceNb;
		putUInt16(pkt->payloadLength, &headerRaw[2]);
		putUInt32(pkt->timestamp, &headerRaw[4]);

		uint32_t crc = crc32(0L, headerRaw, 8);
		pkt->crc = crc;

		free(headerRaw);

		return PKT_OK;
	}

	if (pkt->payload != NULL)
		free(pkt->payload);
	pkt->payload = malloc(length);

	int i;
	for (i=0; i<length; i++)
		pkt->payload[i] = data[i];

	pkt->payloadLength = length;

	//Computation of the CRC32
	uint8_t* headerAndPayload = malloc(length+8);
	headerAndPayload[0] = (pkt->type << 5) | pkt->window;
	headerAndPayload[1] = pkt->sequenceNb;
	putUInt16(pkt->payloadLength, &headerAndPayload[2]);
	putUInt32(pkt->timestamp, &headerAndPayload[4]);
	for (i=0; i<length; i++)
		headerAndPayload[i+8] = data[i];

	uint32_t crc = crc32(0L, headerAndPayload, length+8);
	pkt->crc = crc;

	free(headerAndPayload);

	return PKT_OK;
}
