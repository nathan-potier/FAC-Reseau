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

// retournent la taille du message généré en octets pour l'envoi par le send:
// CODEREQ = 3
size_t serialize_creer_groupe(const req_creer_groupe_t *req, uint8_t *buffer);

// Désérialise la réponse du serveur apres la création d'un groupe (CODEREQ=4)
void deserialize_creer_groupe_resp(const uint8_t *buffer, rep_creer_groupe_t *rep);

// CODEREQ = 5
size_t serialize_inviter(const req_inviter_t *req, uint8_t *buffer);

// CODEREQ = 6
size_t serialize_lister_invites(const req_lister_invites_t *req, uint8_t *buffer);

// CODEREQ = 7
void deserialize_lister_invites_resp(const uint8_t *buffer, rep_lister_invites_t *rep);

// CODEREQ = 8
size_t serialize_action_invitation(const req_action_invitation_t *req, uint8_t *buffer);

// CODEREQ = 24
void deserialize_inviter_resp(const uint8_t *buffer, rep_inviter_t *rep);