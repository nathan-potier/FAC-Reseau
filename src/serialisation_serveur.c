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