#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#define REQ_INSCRIPTION_SIZE 127 // 2 (codereq) + 2 (zeros) + 10 (nom) + 113 (cle)
#define RESP_INSCRIPTION_SIZE 119 // 2 (codereq) + 2 (id) + 2 (portudp) + 113 (cle)

// Serialisation Requête d'inscription (CODEREQ=1)
void serialize_inscription_req(const char *pseudo, uint8_t *buffer);

// Désérialise la réponse du serveur (CODEREQ=2) pour récupérer l'ID et le port UDP
void deserialize_inscription_resp(const uint8_t *buffer, uint16_t *out_id, uint16_t *out_portudp);