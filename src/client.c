#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "utils.h"
#include <unistd.h>
#include "serialisation_client.h" 

int main() {
    char pseudo[100];
    
    req_inscription_t requete;
    rep_inscription_t reponse;
    
    uint8_t buffer_envoi[REQ_INSCRIPTION_SIZE];
    uint8_t buffer_reception[RESP_INSCRIPTION_SIZE];

    memset(&requete, 0, sizeof(requete));
    memset(&reponse, 0, sizeof(reponse));
    memset(buffer_envoi, 0, REQ_INSCRIPTION_SIZE);
    memset(buffer_reception, 0, RESP_INSCRIPTION_SIZE);

    printf("BIENVENUE SUR PAROLES\n");
    printf("Entrez votre pseudo (10 caractères max) : ");
    
    if (fgets(pseudo, sizeof(pseudo), stdin) != NULL) {
        pseudo[strcspn(pseudo, "\n")] = 0; //remplace le \n par \0
        
        if (strlen(pseudo) == 0) {
            printf("Erreur : Le pseudo ne peut pas être vide.\n");
            return 1;
        }
    } else {
        printf("Erreur de lecture.\n");
        return 1;
    }

    requete.codereq = 1;
    requete.zeros = 0;
    
    memset(requete.nom, 0, 10);
    strncpy(requete.nom, pseudo, 10);
    
    // Clé vide (sans sécurité)
    memset(requete.cle, 0, 113); 

    serialize_inscription_req(&requete, buffer_envoi);
    
    int sock = init_client_tcp_ipv6("::1", 8080);
    if (sock < 0) {
        printf("Erreur de connexion au serveur.\n");
        return 1;
    } 
    

    my_send(sock, buffer_envoi, REQ_INSCRIPTION_SIZE);
    my_recv(sock, buffer_reception, RESP_INSCRIPTION_SIZE);

    close(sock);
    
    deserialize_inscription_resp(buffer_reception, &reponse);

    if (reponse.codereq == 2) {
        printf("INSCRIPTION RÉUSSIE !\n");
        printf("ID : %u\n", reponse.id);
        printf("Port UDP : %u\n", reponse.portudp);
    } else {
        printf("\n[Erreur] Le serveur a refusé l'inscription (Code retour : %u).\n", reponse.codereq);
    }


    uint16_t mon_id = reponse.id; // sauvegarde ID
    char commande[256];

    printf("\n=== MENU ===\n");
    printf("create <nom_groupe>\n");
    printf("invite <id_groupe> <id_dest1> [id_dest2] [id_dest3]...\n");
    printf("li\n"); // lister les invités
    printf("join <id_groupe>\n");
    printf("decline <id_groupe>\n");
    printf("quit\n");
    printf("============\n\n");

    while(1){
        printf("> ");
        if (fgets(commande, sizeof(commande), stdin) == NULL) break;
        commande[strcspn(commande, "\n")] = 0;

        if (strcmp(commande, "quit") == 0) {
            printf("Fermeture du client\n");
            break;
        }

        else{
            if (strlen(commande) > 0) printf("Commande inconnue.\n");
        }
    }


    return 0;
}