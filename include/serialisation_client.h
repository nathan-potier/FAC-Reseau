#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#include "codereq.h"

// Serialisation Requête d'inscription (CODEREQ=1)
void serialize_inscription_req(const req_inscription_t *req, uint8_t *buffer);

// Désérialise la réponse du serveur (CODEREQ=2) pour récupérer l'ID et le port UDP
void deserialize_inscription_resp(const uint8_t *buffer, rep_inscription_t *rep);