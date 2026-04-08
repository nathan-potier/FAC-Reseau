#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "utils.h"
#include <unistd.h>
#include <pthread.h>
#include "serialisation_client.h" 
#include "utils_client.h" 

int main() {
    uint16_t mon_id = 0;
    uint16_t mon_port_udp = 0;
    char cle_serveur[113];

    printf("BIENVENUE SUR PAROLES\n");
    

    if (!initialiser_profil(&mon_id, &mon_port_udp, cle_serveur)) {
        return 1;
    }

    char commande[256];

    // Ecoute des notif personnelles
    int sock_udp = init_ecoute_udp_ipv6(mon_port_udp);
    if (sock_udp >= 0) {
        pthread_t thread_udp;
        pthread_create(&thread_udp, NULL, ecoute_notifications_udp, &sock_udp);
        printf("Écoute des notifications UDP activée sur le port %u.\n", mon_port_udp);
    }

    printf("\n=== MENU ===\n");
    printf("create <nom_groupe>\n");
    printf("invite <id_groupe> <id_dest1> [id_dest2] [id_dest3]...\n");
    printf("li (lister les invités)\n"); // lister les invités
    printf("join <id_groupe>\n");
    printf("decline <id_groupe>\n");
    printf("leave <id_groupe>\n");
    printf("members <id_groupe> (0 pour tous)\n");
    printf("post <id_groupe> <message>\n");
    printf("quit\n");
    printf("============\n\n");

    while(1){
        printf("> ");
        if (fgets(commande, sizeof(commande), stdin) == NULL) break;
        commande[strcspn(commande, "\n")] = 0;

        if (strcmp(commande, "quit") == 0) {
            printf("Fermeture du client\n");
            break;

        } else if (strncmp(commande, "create ", 7) == 0) {
            cmd_create(commande + 7, mon_id);

        } else if (strncmp(commande, "invite ", 7) == 0) {
            cmd_invite(commande + 7, mon_id);

        } else if (strcmp(commande, "li") == 0) {
            cmd_li(mon_id);
            
        } else if (strncmp(commande, "join ", 5) == 0) {
            cmd_join(commande + 5, mon_id);

        } else if (strncmp(commande, "decline ", 8) == 0) {
            cmd_decline(commande + 8, mon_id);

        } else if (strncmp(commande, "leave ", 6) == 0) { 
            cmd_leave(commande + 6, mon_id);

        } else if (strncmp(commande, "members ", 8) == 0) {
            cmd_members(commande + 8, mon_id);

        } else if (strncmp(commande, "post ", 5) == 0) {
            cmd_post(commande + 5, mon_id);
            
        }else{
            if (strlen(commande) > 0) printf("Commande inconnue.\n");
        }
    }


    return 0;
}