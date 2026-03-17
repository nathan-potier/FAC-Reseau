#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#define REQ_INSCRIPTION_SIZE 125 // 2 (codereq + zeros) + 10 (nom) + 113 (cle)
#define RESP_INSCRIPTION_SIZE 117 // 2 (codereq + id) + 2 (portudp) + 113 (cle)

// Requête du client -> serveur (CODEREQ = 1)
typedef struct {
    uint8_t codereq;      // 1
    uint8_t zeros;        // 0
    char nom[10];         // Pseudo (complété par des \0 si < 10)
    char cle[113];        // Clé
} req_inscription_t;

// Réponse du serveur -> client (CODEREQ = 2)
typedef struct {
    uint8_t codereq;      // 2
    uint8_t id;           // Id unique
    uint16_t portudp;     // Port UDP
    char cle[113];        // Clé
} rep_inscription_t;

// Serialisation Requête d'inscription (CODEREQ=1)
void serialize_inscription_req(const char *pseudo, uint8_t *buffer);

// Désérialise la réponse du serveur (CODEREQ=2) pour récupérer l'ID et le port UDP
void deserialize_inscription_resp(const uint8_t *buffer, uint16_t *out_id, uint16_t *out_portudp);