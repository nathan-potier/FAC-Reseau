#ifndef SERIALISATION_SERVEUR_H
#define SERIALISATION_SERVEUR_H

#include <stdint.h>
#include "serialisation_client.h"

// Réponse du serveur -> client (CODEREQ = 2)
typedef struct {
    uint8_t codereq;      // 2
    uint8_t id;           // Id unique
    uint16_t portudp;     // Port UDP
    char cle[113];        // Clé
} rep_inscription_t;

// Réponse du serveur -> client pour création de groupe (CODEREQ = 4)
typedef struct {
    uint8_t codereq;
    uint16_t idg;
    uint16_t portmdiff;
    uint8_t ipmdiff[16];
} rep_creer_groupe_t;

void deserialize_inscription_req(const uint8_t *buffer, req_inscription_t *req);
void serialize_inscription_resp(const rep_inscription_t *rep, uint8_t *buffer);

void deserialize_creer_groupe_req(const uint8_t *buffer, req_creer_groupe_t *req);
void serialize_creer_groupe_resp(const rep_creer_groupe_t *rep, uint8_t *buffer);

#endif