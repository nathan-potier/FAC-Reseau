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

// CodeReq 6
void deserialize_inviter_req(const uint8_t *buffer, req_inviter_t *req);
void deserialize_lister_invites_req(const uint8_t *buffer, req_lister_invites_t *req);
size_t serialize_lister_invites_resp(const rep_lister_invites_t *rep, uint8_t *buffer);

void deserialize_action_invitation_req(const uint8_t *buffer, req_action_invitation_t *req);
size_t serialize_accepter_invitation_resp(const rep_action_invitation_acceptee_t *rep, uint8_t *buffer);

void deserialize_lister_membres_req(const uint8_t *buffer, req_demander_membres_t *req);
size_t serialize_lister_membres_resp(const rep_demander_membres_t *rep, uint8_t *buffer);

//messagerie 
void deserialize_poster_billet_req(const uint8_t *buffer, req_poster_billet_t *req);
void serialize_poster_billet_resp(const rep_poster_billet_t *rep, uint8_t *buffer);

void deserialize_repondre_billet_req(const uint8_t *buffer, req_repondre_billet_t *req);
void serialize_repondre_billet_resp(const rep_repondre_billet_t *rep, uint8_t *buffer);

void deserialize_demander_billets_req(const uint8_t *buffer, req_demander_billets_t *req);


#endif