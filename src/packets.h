#ifndef _PACKETS_H_
#define _PACKETS_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <zlib.h>
#include <arpa/inet.h>

#include "defines.h"

#define MAX_WINDOW_SIZE 31

typedef struct pkt pkt_t;
struct __attribute__((__packed__)) pkt//everything is kept in host byte-order
{
	uint32_t type : 3,//type (see enum ptypes_t)
				window : 5,//the current state of the sender's window
				sequenceNb : 8,
				payloadLength : 16;//in bytes (<= MAX_PAYLOAD_SIZE)
	uint32_t timestamp;
	uint8_t* payload;//the data to be transmitted
	uint32_t crc;//cyclic redundancy check (calculated with zlib's function crc32) computed on the header and payload FROM THE NETWORK (network byte-order)
};

typedef enum {
	PTYPE_DATA = 1,
	PTYPE_ACK = 2,
} ptypes_t;

typedef enum {
	PKT_OK = 0,     /* Le paquet a Ã©tÃ© traitÃ© avec succÃ¨s */
	E_TYPE,         /* Erreur liÃ©e au champs Type */
	E_LENGTH,       /* Erreur liÃ©e au champs Length  */
	E_CRC,          /* CRC invalide */
	E_WINDOW,       /* Erreur liÃ©e au champs Window */
	E_SEQNUM,       /* NumÃ©ro de sÃ©quence invalide */
	E_NOMEM,        /* Pas assez de mÃ©moire */
	E_NOHEADER,     /* Le paquet n'a pas de header (trop court) */
	E_UNCONSISTENT, /* Le paquet est incohÃ©rent */
} pkt_status_code;



uint16_t getUInt16(const uint8_t* buf);
void putUInt16(uint16_t nb, uint8_t* buf);
uint32_t getUInt32(const uint8_t* buf);
void putUInt32(uint32_t nb, uint8_t* buf);

/* Alloue et initialise une struct pkt
 * @return: NULL en cas d'erreur */
pkt_t* pkt_new();
/* LibÃ¨re le pointeur vers la struct pkt, ainsi que toutes les
 * ressources associÃ©es
 */
void pkt_del(pkt_t*);

/*
 * DÃ©code des donnÃ©es reÃ§ues et crÃ©e une nouvelle structure pkt.
 * Le paquet reÃ§u est en network byte-order.
 * La fonction vÃ©rifie que:
 * - Le CRC32 des donnÃ©es reÃ§ues est le mÃªme que celui dÃ©codÃ© Ã  la fin
 *   du flux de donnÃ©es
 * - Le type du paquet est valide
 * - La longeur du paquet est valide et cohÃ©rente avec le nombre d'octets
 *   reÃ§us.
 *
 * @data: L'ensemble d'octets constituant le paquet reÃ§u
 * @len: Le nombre de bytes reÃ§us
 * @pkt: Une struct pkt valide
 * @post: pkt est la reprÃ©sentation du paquet reÃ§u
 *
 * @return: Un code indiquant si l'opÃ©ration a rÃ©ussi ou reprÃ©sentant
 *         l'erreur rencontrÃ©e.
 */
pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt);

/*
 * Encode une struct pkt dans un buffer, prÃªt Ã  Ãªtre envoyÃ© sur le rÃ©seau
 * (c-Ã -d en network byte-order), incluant le CRC32 du header et payload.
 *
 * @pkt: La structure Ã  encoder
 * @buf: Le buffer dans lequel la structure sera encodÃ©e
 * @len: La taille disponible dans le buffer
 * @len-POST: Le nombre de d'octets Ã©crit dans le buffer
 * @return: Un code indiquant si l'opÃ©ration a rÃ©ussi ou E_NOMEM si
 *         le buffer est trop petit.
 */
pkt_status_code pkt_encode(const pkt_t*, char *buf, size_t *len);

/* Accesseurs pour les champs toujours prÃ©sents du paquet.
 * Les valeurs renvoyÃ©es sont toutes dans l'endianness native
 * de la machine!
 */
ptypes_t pkt_get_type     (const pkt_t*);
uint8_t  pkt_get_window   (const pkt_t*);
uint8_t  pkt_get_seqnum   (const pkt_t*);
uint16_t pkt_get_length   (const pkt_t*);
uint32_t pkt_get_timestamp(const pkt_t*);
uint32_t pkt_get_crc      (const pkt_t*);
/* Renvoie un pointeur vers le payload du paquet, ou NULL s'il n'y
 * en a pas.
 */
const char* pkt_get_payload(const pkt_t*);

/* Setters pour les champs obligatoires du paquet. Si les valeurs
 * fournies ne sont pas dans les limites acceptables, les fonctions
 * doivent renvoyer un code d'erreur adaptÃ©.
 * Les valeurs fournies sont dans l'endianness native de la machine!
 */
pkt_status_code pkt_set_type     (pkt_t*, const ptypes_t type);
pkt_status_code pkt_set_window   (pkt_t*, const uint8_t window);
pkt_status_code pkt_set_seqnum   (pkt_t*, const uint8_t seqnum);
pkt_status_code pkt_set_length   (pkt_t*, const uint16_t length);
pkt_status_code pkt_set_timestamp(pkt_t*, const uint32_t crc);
pkt_status_code pkt_set_crc      (pkt_t*, const uint32_t crc);
/* DÃ©fini la valeur du champs payload du paquet.
 * @data: Une succession d'octets reprÃ©sentants le payload
 * @length: Le nombre d'octets composant le payload
 * @POST: pkt_get_length(pkt) == length */
pkt_status_code pkt_set_payload(pkt_t*,
                                const char *data,
                                const uint16_t length);

#endif //_PACKETS_H_
