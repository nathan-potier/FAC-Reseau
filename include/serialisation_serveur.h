#ifndef SERIALISATION_SERVEUR_H
#define SERIALISATION_SERVEUR_H

#include <stdint.h>
#include "codereq.h"

// Recois CODEREQ=1 du client
void deserialize_inscription_req(const uint8_t *buffer, req_inscription_t *req);
// Pour envoyer CODEREQ=2 au client
void serialize_inscription_resp(const rep_inscription_t *rep, uint8_t *buffer);

// Recois CODEREQ=3 du client
void deserialize_creer_groupe_req(const uint8_t *buffer, req_creer_groupe_t *req);
// Pour envoyer CODEREQ=4 au client
void serialize_creer_groupe_resp(const rep_creer_groupe_t *rep, uint8_t *buffer);

#endif