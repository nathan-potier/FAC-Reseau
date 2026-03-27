#ifndef SERIALISATION_SERVEUR_H
#define SERIALISATION_SERVEUR_H

#include <stdint.h>
#include "serialisation_client.h"

// Requête du client -> serveur pour création de groupe (CODEREQ = 3)
typedef struct {
    uint8_t codereq;
    uint16_t id;
    uint8_t len;
    char nomg[256];
} req_creer_groupe_t;

// Réponse du serveur -> client pour création de groupe (CODEREQ = 4)
typedef struct {
    uint8_t codereq;
    uint16_t idg;
    uint16_t portmdiff;
    uint8_t ipmdiff[16];
} rep_creer_groupe_t;

void deserialize_inscription_req(const uint8_t *buffer, req_inscription_t *req);
void serialize_inscription_resp(const rep_inscription_t *rep, uint8_t *buffer);

#endif