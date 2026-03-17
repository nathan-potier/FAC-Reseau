#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include "serialisation.h"

void serialize_inscription_req(const char *pseudo, uint8_t *buffer) {
    // init buffer à 0
    memset(buffer, 0, REQ_INSCRIPTION_SIZE);
    
    // CODEREQ = 1 en Big-Endian
    uint16_t premiere_ligne = (1 << 11) | 0;
    premiere_ligne = htons(premiere_ligne);
    memcpy(buffer, &premiere_ligne, 2);
        
    // NOM : Copie du pseudo
    strncpy((char *)(buffer + 2), pseudo, 10);
}

void deserialize_inscription_resp(const uint8_t *buffer, uint16_t *out_id, uint16_t *out_portudp) {
    // CODEREQ + ID
    uint16_t premiere_ligne;
    memcpy(&premiere_ligne, buffer, 2);
    premiere_ligne = ntohs(premiere_ligne);

    // decalage 8 bits vers la droite pour le CODEREQ
    uint8_t codereq = premiere_ligne >> 11; 
    uint16_t id = premiere_ligne & 0x07FF; // On masque les 11 bits de poids fort pour ne garder que l'ID
    
    if (codereq != 2) {
        printf("Erreur : Le serveur a renvoyé un code inattendu (%u).\n", codereq);
        return;
    }
    
    // ID reçu
    *out_id = id;
    
    // Port UDP pour les notifs
    uint16_t portudp;
    memcpy(&portudp, buffer + 2, 2);
    *out_portudp = ntohs(portudp);
}