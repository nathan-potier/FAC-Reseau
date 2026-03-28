#ifndef CODEREQ_H
#define CODEREQ_H

#include <stdint.h>

#define REQ_INSCRIPTION_SIZE 125 // 2 (codereq + zeros) + 10 (nom) + 113 (cle)
#define RESP_INSCRIPTION_SIZE 117 // 2 (codereq + id) + 2 (portudp) + 113 (cle)

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

#endif