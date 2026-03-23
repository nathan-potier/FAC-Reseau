#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include "serialisation_client.h"

void serialize_inscription_req(const req_inscription_t *req, uint8_t *buffer) {
    // init buffer à 0
    memset(buffer, 0, REQ_INSCRIPTION_SIZE);
    
    // CODEREQ = 1 en Big-Endian
    uint16_t premiere_ligne = (req->codereq << 11) | 0;
    premiere_ligne = htons(premiere_ligne);
    memcpy(buffer, &premiere_ligne, 2);
        
    // NOM : Copie du pseudo
    strncpy((char *)(buffer + 2), req->nom, 10);

    // Clé
    memcpy(buffer + 12, req->cle, 113);
}


void deserialize_inscription_resp(const uint8_t *buffer, rep_inscription_t *rep) {
    // CODEREQ + ID
    uint16_t premiere_ligne;
    memcpy(&premiere_ligne, buffer, 2);
    premiere_ligne = ntohs(premiere_ligne);

    // decalage 8 bits vers la droite pour le CODEREQ
    rep->codereq = premiere_ligne >> 11; 
    // ID reçu
    rep->id = premiere_ligne & 0x07FF; 
    
    if (rep->codereq != 2) {
        printf("Erreur : Le serveur a renvoyé un code inattendu (%u).\n", rep->codereq);
        return;
    }
    
    // Port UDP
    uint16_t portudp;
    memcpy(&portudp, buffer + 2, 2);
    rep->portudp = ntohs(portudp);
    
    // Copie de la clé du serveur
    memcpy(rep->cle, buffer + 4, 113);
}