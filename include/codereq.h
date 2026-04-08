#ifndef CODEREQ_H
#define CODEREQ_H

#include <stdint.h>

#define REQ_INSCRIPTION_SIZE 125 // 2 (codereq + zeros) + 10 (nom) + 113 (cle)
#define RESP_INSCRIPTION_SIZE 117 // 2 (codereq + id) + 2 (portudp) + 113 (cle)
#define RESP_CREER_GROUPE_SIZE 20

// Requête du client -> serveur pour inscription (CODEREQ = 1)
typedef struct {
    uint8_t codereq;      // 1
    uint8_t zeros;        // 0
    char nom[10];         // Pseudo (complété par des \0 si < 10)
    char cle[113];        // Clé
} req_inscription_t;

// Réponse du serveur -> client pour validé inscription (CODEREQ = 2)
typedef struct {
    uint8_t codereq;      // 2
    uint8_t id;           // Id unique
    uint16_t portudp;     // Port UDP
    char cle[113];        // Clé
} rep_inscription_t;

// Réponse du client -> serveur pour création de groupe (CODEREQ = 3)
typedef struct {
    uint8_t codereq;
    uint16_t id;
    uint8_t len;
    char nomg[256];
} req_creer_groupe_t;

// Réponse du serveur -> client pour création de groupe (CODEREQ = 4)
typedef struct {
    uint8_t codereq;
    uint16_t idg;
    uint16_t portmdiff;
    uint8_t ipmdiff[16];
} rep_creer_groupe_t;

// CODEREQ = 5 (Inviter)
typedef struct {
    uint8_t codereq;      // 5
    uint16_t id_client;   // 11
    uint16_t id_groupe;   // 11
    uint8_t nb;           // 5 bits max donc 31 invités max
    uint16_t id_invites[31]; 
} req_inviter_t;

// CODEREQ = 6 (Demander les invitations)
typedef struct {
    uint8_t codereq;      // 6
    uint16_t id_client;   // 11 bits
} req_lister_invites_t;

// CODEREQ = 7 (Liste des invitations)
typedef struct {
    uint8_t codereq;      
    uint8_t nb;           // Nombre d'invitations (5 bits max)
    uint16_t id_groupes[31]; // Les IDs des groupes (11 bits chacun)
} rep_lister_invites_t;

// CODEREQ = 8 (Accepter/Refuser/Quitter)
typedef struct {
    uint8_t codereq;      // 5
    uint16_t id_client;   // 11 bits
    uint16_t id_groupe;   // 11 bits
    uint8_t an;           // 1 = Accepte, 0 = Refuse, 2 = Quitte sur 2 bits
} req_action_invitation_t;

// Réponse du serveur -> client suite à une invitation acceptée (CODEREQ = 9)
typedef struct {
    uint8_t codereq;               // 5 bits
    uint16_t idg;                  // 11 bits
    uint16_t portmdiff;            // 16 bits
    uint8_t ipmdiff[16];           // 16 octets
    uint16_t nb;                   // 16 bits
    uint16_t id_membres[100];      // 11 bits chacun
    char nom_membres[100][10];     // Liste des pseudos (10 octets)
} rep_action_invitation_acceptee_t;

// Réponse du serveur -> client suite à une invitation (CODEREQ = 24)
typedef struct {
    uint8_t codereq;      // 24 (sur 5 bits)
    uint16_t zeros;       // 0 (sur 11 bits)
} rep_inviter_t;

// CODEREQ = 10 (Demander la liste des membres)
typedef struct {
    uint8_t codereq;      // 10 (sur 5 bits)
    uint16_t id_client;   // 11 bits
    uint16_t id_groupe;   // 11 bits
} req_demander_membres_t;

// CODEREQ = 11 serveur -> client
typedef struct {
    uint8_t codereq;               // 11
    uint16_t idg;                  // 1 bits
    uint16_t nb;                   // 6 bits
    uint16_t id_membres[100];
    char nom_membres[100][10];     // 10 octets chacun
} rep_demander_membres_t;

// CODEREQ = 12 (Poster un billet)
typedef struct {
    uint8_t codereq;      // 5 bits
    uint16_t id_client;   // 11 bits
    uint16_t id_groupe;   // 11 bits
    uint16_t len;          // Longueur du billet
    char data[2048];       // Contenu du billet
} req_poster_billet_t;

// CODEREQ = 13 reponse serveur -> client apres post d'un billet
typedef struct {
    uint8_t codereq;      // 5 bits
    uint16_t idg;         // 11 bits
    uint16_t numb;        // Numéro du billet donné par le serveur (16 bits)
} rep_poster_billet_t;

// CODEREQ = 14 Répondre à un billet
typedef struct {
    uint8_t codereq;      
    uint16_t id_groupe; //11 bits 
    uint16_t id_client; //11 bits
    uint16_t numb; //11 btis
    uint16_t len; //16bits     
    char data[2048];
} req_repondre_billet_t;

// CODEREQ = 15 Réponse suite à une réponse de billet
typedef struct {
    uint8_t codereq;      
    uint16_t id_groupe; //11 bits    
    uint16_t numb; // 11 bits
    uint16_t numr;    // 5bits
} rep_repondre_billet_t;

// CODEREQ = 16 Demander les derniers billets d'un groupe
typedef struct {
    uint8_t codereq;      
    uint16_t id_groupe;  //11       
    uint16_t id_client;   //11
    uint16_t numb;        //11
    uint16_t numr;        //5
} req_demander_billets_t;
#endif