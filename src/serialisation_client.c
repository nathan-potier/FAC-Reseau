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

size_t serialize_demander_membres(const req_demander_membres_t *req, uint8_t *buffer) {
    // CODEREQ (5 bits) + ID_CLIENT (11 bits)
    uint16_t premiere_ligne = (req->codereq << 11) | (req->id_client & 0x07FF);
    premiere_ligne = htons(premiere_ligne);
    memcpy(buffer, &premiere_ligne, 2);
    
    // IDG (11 bits) + ZEROS (5 bits)
    uint16_t ligne2 = (req->id_groupe << 5) | 0; 
    ligne2 = htons(ligne2);
    memcpy(buffer + 2, &ligne2, 2);
    
    return 4;
}

void deserialize_demander_membres_resp(const uint8_t *buffer, rep_demander_membres_t *rep) {
    //CODEREQ (5 bits) + IDG (11 bits)
    uint16_t ligne1;
    memcpy(&ligne1, buffer, 2);
    ligne1 = ntohs(ligne1);
    
    rep->codereq = ligne1 >> 11;
    rep->idg = ligne1 & 0x07FF;
    
    // NB : 16 bits
    uint16_t ligne2;
    memcpy(&ligne2, buffer + 2, 2);
    rep->nb = ntohs(ligne2);
    
    size_t decalage = 4; // On a déjà lu 4 octets
    
    for (int i = 0; i < rep->nb; i++) {
        // ID_membre (11 bits) + ZEROS (5 bits)
        uint16_t ligne_id;
        memcpy(&ligne_id, buffer + decalage, 2);
        ligne_id = ntohs(ligne_id);
        
        rep->id_membres[i] = ligne_id >> 5;
        decalage += 2;
        
        // Nom du membre (10 octets)
        memset(rep->nom_membres[i], 0, 10);
        memcpy(rep->nom_membres[i], buffer + decalage, 10);
        decalage += 10;
    }
}

void deserialize_action_invitation_acceptee_resp(const uint8_t *buffer, rep_action_invitation_acceptee_t *rep) {
    // CODEREQ (5 bits) + IDG (11 bits)
    uint16_t ligne1;
    memcpy(&ligne1, buffer, 2);
    ligne1 = ntohs(ligne1);
    rep->codereq = ligne1 >> 11;
    rep->idg = ligne1 & 0x07FF;

    // PORTMDIFF (16 bits)
    uint16_t port_net;
    memcpy(&port_net, buffer + 2, 2);
    rep->portmdiff = ntohs(port_net);

    // IPMDIFF (16 octets)
    memcpy(rep->ipmdiff, buffer + 4, 16);

    // NB (16 bits)
    uint16_t nb_net;
    memcpy(&nb_net, buffer + 20, 2);
    rep->nb = ntohs(nb_net);

    // Boucle sur les membres (à partir de l'octet 22)
    size_t decalage = 22; 
    
    for (int i = 0; i < rep->nb; i++) {
        // ID_membre (11 bits) + ZEROS (5 bits)
        uint16_t ligne_id;
        memcpy(&ligne_id, buffer + decalage, 2);
        ligne_id = ntohs(ligne_id);
        
        rep->id_membres[i] = ligne_id >> 5; // On enlève les 5 zéros
        decalage += 2;
        
        // Nom du membre (10 octets réseaux)
        memset(rep->nom_membres[i], 0, 10);
        memcpy(rep->nom_membres[i], buffer + decalage, 9);
        decalage += 10;
    }
}

size_t serialize_poster_billet(const req_poster_billet_t *req, uint8_t *buffer) {
    //CODEREQ (5 bits) + ID (11 bits)
    uint16_t ligne1 = (req->codereq << 11) | (req->id_client & 0x07FF);
    ligne1 = htons(ligne1);
    memcpy(buffer, &ligne1, 2);

    // IDG (11 bits) + ZEROS (5 bits)
    uint16_t ligne2 = (req->id_groupe << 5) | 0;
    ligne2 = htons(ligne2);
    memcpy(buffer + 2, &ligne2, 2);

    // LEN (2 octets)
    uint16_t len_net = htons(req->len);
    memcpy(buffer + 4, &len_net, 2);

    // Ligne 4 et + : DATA (Commence à l'octet 6)
    memcpy(buffer + 6, req->data, req->len);

    return 6 + req->len; // 6 octets d'en-tête + la taille du texte
}

void deserialize_poster_billet_resp(const uint8_t *buffer, rep_poster_billet_t *rep) {
    // CODEREQ (5 bits) + IDG (11 bits)
    uint16_t ligne1;
    memcpy(&ligne1, buffer, 2);
    ligne1 = ntohs(ligne1);
    rep->codereq = ligne1 >> 11;
    rep->idg = ligne1 & 0x07FF;

    // NUMB (11 bits) + ZEROS (5 bits)
    uint16_t ligne2;
    memcpy(&ligne2, buffer + 2, 2);
    ligne2 = ntohs(ligne2);
    rep->numb = ligne2 >> 5; // 5 bits de zéros à droite
}

size_t serialize_repondre_billet(const req_repondre_billet_t *req, uint8_t *buffer) {
    // CODEREQ (5 bits) + ID (11 bits)
    uint16_t ligne1 = (req->codereq << 11) | (req->id_client & 0x07FF);
    ligne1 = htons(ligne1);
    memcpy(buffer, &ligne1, 2);

    // IDG (11 bits) + ZEROS (5 bits)
    uint16_t ligne2 = (req->id_groupe << 5) | 0;
    ligne2 = htons(ligne2);
    memcpy(buffer + 2, &ligne2, 2);

    // NUMB (11 bits) + ZEROS (5 bits)
    uint16_t ligne3 = (req->numb << 5) | 0;
    ligne3 = htons(ligne3);
    memcpy(buffer + 4, &ligne3, 2);

    // LEN (16 bits)
    uint16_t len_net = htons(req->len);
    memcpy(buffer + 6, &len_net, 2);

    // DATA (Commence à l'octet 8)
    memcpy(buffer + 8, req->data, req->len);

    return 8 + req->len; // 8 octets d'en-tête + taille du texte
}

void deserialize_repondre_billet_resp(const uint8_t *buffer, rep_repondre_billet_t *rep) {
    // CODEREQ (5 bits) + IDG (11 bits)
    uint16_t ligne1;
    memcpy(&ligne1, buffer, 2);
    ligne1 = ntohs(ligne1);
    rep->codereq = ligne1 >> 11;
    rep->id_groupe = ligne1 & 0x07FF;

    // NUMB (11 bits) + NUMR (5 bits)
    uint16_t ligne2;
    memcpy(&ligne2, buffer + 2, 2);
    ligne2 = ntohs(ligne2);
    rep->numb = ligne2 >> 5;
    rep->numr = ligne2 & 0x1F; // 0x1F = 11111
}