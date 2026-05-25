#include <string.h>
#include <arpa/inet.h>
#include "serialisation_serveur.h"

// Désérialisation : Buffer réseau -> Structure locale
void deserialize_inscription_req(const uint8_t *buffer, req_inscription_t *req) {
    uint16_t premiere_ligne;
    memcpy(&premiere_ligne, buffer, 2);
    
    
    premiere_ligne = ntohs(premiere_ligne);

    req->codereq = premiere_ligne >> 11;

    // Extraction des 11 suivants (ZEROS) 
    req->zeros = premiere_ligne & 0x07FF;

    // Récupération du pseudo et de la clé 
    memcpy(req->nom, buffer + 2, 10);
    memcpy(req->cle, buffer + 12, 113);
}

// Sérialisation : Structure locale -> Buffer réseau
void serialize_inscription_resp(const rep_inscription_t *rep, uint8_t *buffer) {
    memset(buffer, 0, RESP_INSCRIPTION_SIZE); 

    // Concaténation de CODEREQ et ID sur un entier de 16 bits
    uint16_t premiere_ligne = (rep->codereq << 11) | (rep->id & 0x07FF);
    
    // Conversion au format réseau (Big-Endian) avant l'envoi
    premiere_ligne = htons(premiere_ligne);
    memcpy(buffer, &premiere_ligne, 2);

    // Conversion du port UDP (2 octets) en Big-Endian
    uint16_t portudp_net = htons(rep->portudp);
    memcpy(buffer + 2, &portudp_net, 2);

    memcpy(buffer + 4, rep->cle, 113);
}


// Désérialise la requête de création de groupe (CODEREQ = 3)
void deserialize_creer_groupe_req(const uint8_t *buffer, req_creer_groupe_t *req) {
    uint16_t premiere_ligne;
    memcpy(&premiere_ligne, buffer, 2);
    premiere_ligne = ntohs(premiere_ligne); 

    req->codereq = premiere_ligne >> 11;
    req->id = premiere_ligne & 0x07FF;

    req->len = buffer[2];
    memset(req->nomg, 0, 256);
    memcpy(req->nomg, buffer + 3, req->len);
}

// Sérialise la réponse de création de groupe (CODEREQ = 4)
void serialize_creer_groupe_resp(const rep_creer_groupe_t *rep, uint8_t *buffer) {
    memset(buffer, 0, 20);

    uint16_t premiere_ligne = htons((rep->codereq << 11) | (rep->id_groupe & 0x07FF));
    memcpy(buffer, &premiere_ligne, 2);

    uint16_t port_net = htons(rep->portmdiff);
    memcpy(buffer + 2, &port_net, 2);

    memcpy(buffer + 4, rep->ipmdiff, 16);
}

// Désérialise la requête d'invitation (CODEREQ = 5)
void deserialize_inviter_req(const uint8_t *buffer, req_inviter_t *req) {
    uint16_t l1, l2;
    memcpy(&l1, buffer, 2); l1 = ntohs(l1);
    req->codereq = l1 >> 11;
    req->id_client = l1 & 0x07FF;

    memcpy(&l2, buffer + 2, 2); l2 = ntohs(l2);
    req->id_groupe = l2 >> 5;
    req->nb = l2 & 0x1F;

    for(int i=0; i<req->nb; i++) {
        uint16_t id;
        memcpy(&id, buffer + 4 + (i*2), 2);
        req->id_invites[i] = ntohs(id) >> 5;
    }
}

// Désérialise la requête de listing des invitations (CODEREQ = 6)
void deserialize_lister_invites_req(const uint8_t *buffer, req_lister_invites_t *req) {
    uint16_t l1;
    memcpy(&l1, buffer, 2); l1 = ntohs(l1);
    req->codereq = l1 >> 11;
    req->id_client = l1 & 0x07FF;
}


// Sérialise la réponse de listing des invitations (CODEREQ = 7)
size_t serialize_lister_invites_resp(const rep_lister_invites_t *rep, uint8_t *buffer) {
    uint16_t l1 = htons((rep->codereq << 11) | rep->nb);
    memcpy(buffer, &l1, 2);
    int offset = 2;
    for (int i = 0; i < rep->nb; i++) {
        // IDG1 (11 bits) + LEN1 (5 bits)
        uint16_t ligne_g = htons((rep->id_groupes[i] << 5) | (rep->len_groupes[i] & 0x1F));
        memcpy(buffer + offset, &ligne_g, 2);
        offset += 2;
        
        // NOMG1
        memcpy(buffer + offset, rep->nom_groupes[i], rep->len_groupes[i]);
        offset += rep->len_groupes[i];
        
        // NOM1 (Admin) 10 octets
        memcpy(buffer + offset, rep->nom_admins[i], 10);
        offset += 10;
    }
    return offset;
}

// Désérialise la requête d'action sur invitation (Accepter, Refuser, Quitter) (CODEREQ = 8)
void deserialize_action_invitation_req(const uint8_t *buffer, req_action_invitation_t *req) {
    uint16_t l1, l2;
    memcpy(&l1, buffer, 2); l1 = ntohs(l1);
    req->codereq = l1 >> 11;
    req->id_client = l1 & 0x07FF;

    memcpy(&l2, buffer + 2, 2); l2 = ntohs(l2);
    req->id_groupe = l2 >> 5;
    req->an = (l2 >> 3) & 0x03;
}

// Désérialise la réponse à une invitation (CODEREQ = 24)
size_t serialize_accepter_invitation_resp(const rep_action_invitation_acceptee_t *rep, uint8_t *buffer) {
    uint16_t l1 = htons((rep->codereq << 11) | (rep->id_groupe & 0x07FF));
    memcpy(buffer, &l1, 2);
    
    uint16_t port = htons(rep->portmdiff);
    memcpy(buffer + 2, &port, 2);
    memcpy(buffer + 4, rep->ipmdiff, 16);
    
    uint16_t nb = htons(rep->nb);
    memcpy(buffer + 20, &nb, 2);
    
    int offset = 22;
    for(int i=0; i < rep->nb; i++) {
        uint16_t id = htons(rep->id_membres[i] << 5);
        memcpy(buffer + offset, &id, 2);
        offset += 2;
        memcpy(buffer + offset, rep->nom_membres[i], 10);
        offset += 10;
    }
    return offset;
}

// Désérialise la requête de listing des membres d'un groupe (CODEREQ = 10)
void deserialize_lister_membres_req(const uint8_t *buffer, req_demander_membres_t *req) {
    uint16_t l1, l2;
    memcpy(&l1, buffer, 2); l1 = ntohs(l1);
    req->codereq = l1 >> 11;
    req->id_client = l1 & 0x07FF;

    memcpy(&l2, buffer + 2, 2); l2 = ntohs(l2);
    req->id_groupe = l2 >> 5;
}

// Sérialise la réponse de listing des membres d'un groupe (CODEREQ = 11)
size_t serialize_lister_membres_resp(const rep_demander_membres_t *rep, uint8_t *buffer) {
    uint16_t l1 = htons((rep->codereq << 11) | (rep->id_groupe & 0x07FF));
    memcpy(buffer, &l1, 2);
    
    uint16_t nb = htons(rep->nb);
    memcpy(buffer + 2, &nb, 2);
    
    int offset = 4;
    for(int i=0; i < rep->nb; i++) {
        uint16_t id = htons(rep->id_membres[i] << 5);
        memcpy(buffer + offset, &id, 2);
        offset += 2;
        memcpy(buffer + offset, rep->nom_membres[i], 10);
        offset += 10;
    }
    return offset;
}


//codereq 12
void deserialize_poster_billet_req(const uint8_t *buffer, req_poster_billet_t *req) {
    uint16_t l1, l2, l3;
    memcpy(&l1, buffer, 2); l1 = ntohs(l1);
    req->codereq = l1 >> 11;
    req->id_client = l1 & 0x07FF;

    memcpy(&l2, buffer + 2, 2); l2 = ntohs(l2);
    req->id_groupe = l2 >> 5;

    memcpy(&l3, buffer + 4, 2); 
    req->len = ntohs(l3);

    if (req->len > sizeof(req->data)) {
        req->len = sizeof(req->data); 
    }
    memset(req->data, 0, sizeof(req->data));
    memcpy(req->data, buffer + 6, req->len); 
}


//codereq 13
void serialize_poster_billet_resp(const rep_poster_billet_t *rep, uint8_t *buffer) {
    uint16_t l1 = htons((rep->codereq << 11) | (rep->id_groupe & 0x07FF));
    memcpy(buffer, &l1, 2);
    
    uint16_t l2 = htons(rep->numb << 5); 
    memcpy(buffer + 2, &l2, 2);
    
}

//codereq 14
void deserialize_repondre_billet_req(const uint8_t *buffer, req_repondre_billet_t *req) {
    uint16_t l1, l2, l3, l4;
    memcpy(&l1, buffer, 2); l1 = ntohs(l1);
    req->codereq = l1 >> 11;
    req->id_client = l1 & 0x07FF;

    memcpy(&l2, buffer + 2, 2); l2 = ntohs(l2);
    req->id_groupe = l2 >> 5;

    memcpy(&l3, buffer + 4, 2); 
    req->numb = ntohs(l3) >> 5;

    memcpy(&l4, buffer + 6, 2); 
    req->len = ntohs(l4);

    if (req->len > sizeof(req->data)) {
        req->len = sizeof(req->data); 
    }
    memset(req->data, 0, sizeof(req->data));
    memcpy(req->data, buffer + 8, req->len); 
}

//codereq 15
void serialize_repondre_billet_resp(const rep_repondre_billet_t *rep, uint8_t *buffer) {
    uint16_t l1 = htons((rep->codereq << 11) | (rep->id_groupe & 0x07FF));
    memcpy(buffer, &l1, 2);
    
    uint16_t l2 = htons((rep->numb << 5) | (rep->numr & 0x1F));
    memcpy(buffer + 2, &l2, 2);
}

//codereq 16
void deserialize_demander_billets_req(const uint8_t *buffer, req_demander_billets_t *req) {
    uint16_t l1, l2, l3;
    memcpy(&l1, buffer, 2); l1 = ntohs(l1);
    req->codereq = l1 >> 11;
    req->id_client = l1 & 0x07FF;

    memcpy(&l2, buffer + 2, 2); l2 = ntohs(l2);
    req->id_groupe = l2 >> 5;

    memcpy(&l3, buffer + 4, 2); l3 = ntohs(l3);
    req->numb = l3 >> 5;
    req->numr = l3 & 0x1F;
}