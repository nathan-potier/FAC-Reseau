#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

void serialize_inscription_req(const char *pseudo, uint8_t *buffer) {
    // init buffer à 0
    memset(buffer, 0, REQ_INSCRIPTION_SIZE);
    
    // CODEREQ = 1 en Big-Endian
    uint16_t codereq = htons(1);
    memcpy(buffer, &codereq, sizeof(codereq));
        
    // NOM : Copie du pseudo
    strncpy((char *)(buffer + 4), pseudo, 10);
}

void deserialize_inscription_resp(const uint8_t *buffer, uint16_t *out_id, uint16_t *out_portudp) {
    uint16_t codereq;
    
    memcpy(&codereq, buffer, sizeof(codereq));
    codereq = ntohs(codereq);
    
    if (codereq != 2) {
        printf("Erreur : Le serveur a renvoyé un code inattendu (%u).\n", codereq);
        return;
    }
    
    // ID reçu
    memcpy(out_id, buffer + 2, sizeof(*out_id));
    *out_id = ntohs(*out_id);
    
    // Port UDP pour les notifs
    memcpy(out_portudp, buffer + 4, sizeof(*out_portudp));
    *out_portudp = ntohs(*out_portudp);
}