#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include "utils_client.h"

void *ecoute_notifications_udp(void *arg) {
    int sock_udp = *((int *)arg);
    uint8_t buffer[512];

    while (1) {
        int recu = recvfrom(sock_udp, buffer, sizeof(buffer), 0, NULL, NULL);
        if (recu >= 2) {
            uint16_t entete;
            memcpy(&entete, buffer, 2);
            entete = ntohs(entete);
            uint8_t codereq = entete >> 11;

            // Invitation (CODEREQ = 22)
            if (codereq == 22) {
                uint16_t idg = entete & 0x07FF;
                printf("\n\nTu as reçu une INVITATION pour le groupe ID : %u !\n", idg);
                printf("Tape 'li' pour voir tes invitations ou 'join %u' pour accepter.\n> ", idg);
                fflush(stdout);
            }
        }
    }
    return NULL;
}