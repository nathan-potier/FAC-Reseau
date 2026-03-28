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

size_t serialize_creer_groupe(const req_creer_groupe_t *req, uint8_t *buffer) {
    uint16_t premiere_ligne = (req->codereq << 11) | (req->id & 0x07FF);
    premiere_ligne = htons(premiere_ligne);
    memcpy(buffer, &premiere_ligne, 2);

    buffer[2] = req->len;
    memcpy(buffer + 3, req->nomg, req->len);
    
    // 2 octets (premiere_ligne) + 1 octet (len) + x octets (nomg)
    return 3 + req->len; 
}

void deserialize_creer_groupe_resp(const uint8_t *buffer, rep_creer_groupe_t *rep) {
    uint16_t ligne1;
    memcpy(&ligne1, buffer, 2);
    ligne1 = ntohs(ligne1);
    
    rep->codereq = ligne1 >> 11;
    rep->idg = ligne1 & 0x07FF;
    
    // PORTMDIFF
    uint16_t port_net;
    memcpy(&port_net, buffer + 2, 2);
    rep->portmdiff = ntohs(port_net);
    
    // IPMDIFF 
    memcpy(rep->ipmdiff, buffer + 4, 16);
}

size_t serialize_inviter(const req_inviter_t *req, uint8_t *buffer) {
    uint16_t premiere_ligne = (req->codereq << 11) | (req->id_client & 0x07FF);
    premiere_ligne = htons(premiere_ligne);
    memcpy(buffer, &premiere_ligne, 2);
    
    // IDG (11 bits) + NB (5 bits)
    uint16_t ligne2 = (req->id_groupe << 5) | (req->nb & 0x1F); // 0x1F = 11111
    ligne2 = htons(ligne2);
    memcpy(buffer + 2, &ligne2, 2);
    
    // 4 car 2 octets pour la ligne 1 (CODEREQ + ton ID)
    //       2 octets pour la ligne 2 (ID du groupe + nombre d'invités)
    size_t decalage_ligne_invite = 4;
    
    // ID_invite (11 bits) + ZEROS (5 bits)
    for (int i = 0; i < req->nb; i++) {
        uint16_t invite = (req->id_invites[i] << 5) | 0;
        invite = htons(invite);
        memcpy(buffer + decalage_ligne_invite, &invite, 2);
        decalage_ligne_invite += 2;
    }
    
    return decalage_ligne_invite;
}

void deserialize_inviter_resp(const uint8_t *buffer, rep_inviter_t *rep) {
    uint16_t ligne1;
    memcpy(&ligne1, buffer, 2);
    ligne1 = ntohs(ligne1);
    
    rep->codereq = ligne1 >> 11;
    
    rep->zeros = ligne1 & 0x07FF; 
}

size_t serialize_lister_invites(const req_lister_invites_t *req, uint8_t *buffer) {
    uint16_t premiere_ligne = (req->codereq << 11) | (req->id_client & 0x07FF);
    premiere_ligne = htons(premiere_ligne);
    memcpy(buffer, &premiere_ligne, 2);
    
    return 2;
}

void deserialize_lister_invites_resp(const uint8_t *buffer, rep_lister_invites_t *rep) {
    uint16_t ligne1;
    memcpy(&ligne1, buffer, 2);
    ligne1 = ntohs(ligne1);
    
    rep->codereq = ligne1 >> 11;
    rep->nb = (ligne1 >> 6) & 0x1F; 
    
    size_t decalage = 2;
    for(int i = 0; i < rep->nb; i++) {
        uint16_t ligne_groupe;
        memcpy(&ligne_groupe, buffer + decalage, 2);
        ligne_groupe = ntohs(ligne_groupe);
        
        rep->id_groupes[i] = ligne_groupe >> 5;
        decalage += 2;
    }
}

size_t serialize_action_invitation(const req_action_invitation_t *req, uint8_t *buffer) {
    uint16_t premiere_ligne = (req->codereq << 11) | (req->id_client & 0x07FF);
    premiere_ligne = htons(premiere_ligne);
    memcpy(buffer, &premiere_ligne, 2);
    
    // IDG (11 bits) + AN (2 bits) + ZEROS (3 bits)
    uint16_t ligne2 = (req->id_groupe << 5) | ((req->an & 0x03) << 3) | 0;
    ligne2 = htons(ligne2);
    memcpy(buffer + 2, &ligne2, 2);
    
    return 4;
}