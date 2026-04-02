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
            uint16_t idg = entete & 0x07FF;

            
            switch(codereq) {
                case 18: // nv billet ou réponse posté dans un groupe (CODEREQ = 18)
                    printf("\n\nNouveau message sur le fil de discussion groupe ID : %u!\n> ", idg);
                    fflush(stdout);
                    break;
                case 19: // Quelqu'un a rejoint le groupe (CODEREQ = 19)
                    printf("\n\nUn nouvel utilisateur a rejoint le groupe ID : %u !\n> ", idg);
                    fflush(stdout);
                    break;
                    
                case 20: // Quelqu'un a quitté le groupe (CODEREQ = 20)
                    printf("\n\nUn utilisateur a quitté le groupe ID : %u .\n> ", idg);
                    fflush(stdout);
                    break;

                case 21: // L'admin a fermé le groupe (CODEREQ = 21)
                    printf("\n\nL'administrateur a fermé définitivement le groupe ID : %u !\n> ", idg);
                    fflush(stdout);
                    break;

                case 22: // Invitation reçue (CODEREQ = 22)
                    printf("\n\nTu as reçu une INVITATION pour le groupe ID : %u !\n", idg);
                    printf("Tape 'li' pour voir tes invitations ou 'join %u' pour accepter.\n> ", idg);
                    fflush(stdout);
                    break;

                case 23: // Quelqu'un a lu un de tes billets (CODEREQ = 23)
                    printf("\n\nQuelqu'un a récupéré/lu ton billet ! groupe ID : %u \n> ", idg);
                    fflush(stdout);
                    break;
            }
        }
    }
    return NULL;
}