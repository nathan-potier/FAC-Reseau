#ifndef SERIALISATION_SERVEUR_H
#define SERIALISATION_SERVEUR_H

#include <stdint.h>
#include "serialisation_client.h"

void deserialize_inscription_req(const uint8_t *buffer, req_inscription_t *req);
void serialize_inscription_resp(const rep_inscription_t *rep, uint8_t *buffer);

#endif