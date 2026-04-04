#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include "utils_client.h"

#define ADRESSE_LOCALE "::1"
#define PORT_SERVEUR 8080

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

// Codereq = 31
int check_erreur_serveur(const uint8_t *buffer, int recu) {
    if (recu >= 2) {
        uint16_t entete;
        memcpy(&entete, buffer, 2);
        entete = ntohs(entete);
        
        // CODEREQ = 31
        if ((entete >> 11) == 31) {
            uint16_t len = entete & 0x07FF;
            
            if (len > 0) {
                char msg_erreur[2048];
                memset(msg_erreur, 0, sizeof(msg_erreur));
                
                // On s'assure de ne pas lire en dehors de ce qu'on a réellement reçu
                size_t a_lire;
                if (len < (recu - 2)) {
                    a_lire = len;
                } else {
                    a_lire = (recu - 2);
                }
                memcpy(msg_erreur, buffer + 2, a_lire);
                
                printf("\nERREUR SERVEUR : %s\n", msg_erreur);
            } else {
                printf("\nERREUR SERVEUR : Requête refusée.\n");
            }
            return 1;
        }
    }
    return 0;
}

int envoyer_requete_tcp(const uint8_t *buffer_cmd, size_t taille_msg, uint8_t *buffer_rep, size_t taille_max, int taille_exacte) {
    int sock_cmd = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
    if (sock_cmd < 0) {
        printf("Erreur de connexion au serveur.\n");
        return -1;
    }

    my_send(sock_cmd, buffer_cmd, taille_msg);

    int recu;
    if (taille_exacte > 0){
        recu = my_recv(sock_cmd, buffer_rep, taille_exacte);
    }else{
        recu = recv(sock_cmd, buffer_rep, taille_max, 0);
    }
    
    close(sock_cmd);

    if (recu > 0) {
        if (check_erreur_serveur(buffer_rep, recu)) return -1; 
        return recu;
    } else {
        printf("Erreur de réception ou serveur déconnecté.\n");
        return -1;
    }
}

void abonner_groupe_multicast(uint16_t port, const char *ip_str, const char *nom_groupe) {
    // Nouvelle socket sur le port du groupe
    int sock_multi = init_ecoute_udp_ipv6(port);
    if (sock_multi >= 0) {
        rejoindre_multicast_ipv6(sock_multi, ip_str);
        
        // On lance un thread indépendant pour écouter ce groupe
        int *arg_sock_multi = malloc(sizeof(int));
        *arg_sock_multi = sock_multi;
        
        pthread_t thread_multi;
        pthread_create(&thread_multi, NULL, ecoute_notifications_udp, arg_sock_multi);
        pthread_detach(thread_multi);
        
        printf("Écoute Multicast activée pour le groupe \"%s\" sur le port %u.\n", nom_groupe, port);
    } else {
        printf("Erreur lors de la création de l'écoute Multicast.\n");
    }
}