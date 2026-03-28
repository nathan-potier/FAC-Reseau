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
    printf("li (lister les invités)\n"); // lister les invités
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

        else if (strncmp(commande, "create ", 7) == 0) {
            req_creer_groupe_t req_cg;
            req_cg.codereq = 3;
            req_cg.id = mon_id;
            
            char *nom_groupe = commande + 7; // A partir de l'espace arpes la commande
            req_cg.len = strlen(nom_groupe);
            if (req_cg.len > 255){
                req_cg.len = 255;
            }
            strncpy(req_cg.nomg, nom_groupe, req_cg.len);

            uint8_t buffer_cmd[512];
            size_t taille_msg = serialize_creer_groupe(&req_cg, buffer_cmd);

            int sock_cmd = init_client_tcp_ipv6("::1", 8080);
            if (sock_cmd < 0) continue;

            my_send(sock_cmd, buffer_cmd, taille_msg);
            printf("Requete de creation du groupe \"%s\" envoyé.\n", nom_groupe);
            
            uint8_t buffer_rep[512];
            int recu = my_recv(sock_cmd, buffer_rep,RESP_CREER_GROUPE_SIZE );
            close(sock_cmd);

            if (recu > 0) {
                rep_creer_groupe_t rep_cg;
                deserialize_creer_groupe_resp(buffer_rep, &rep_cg);

                if (rep_cg.codereq == 4) {
                    // Transforme l'IP binaire en texte
                    char ip[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, rep_cg.ipmdiff, ip, INET6_ADDRSTRLEN);

                    printf("\nGROUPE CRÉÉ AVEC SUCCÈS !\n");
                    printf("ID du groupe : %u\n", rep_cg.idg);
                    printf("IP Multicast : %s\n", ip);
                    printf("Port Multicast : %u\n\n", rep_cg.portmdiff);
                    
                } else {
                    printf("Erreur : Le serveur a renvoyé un code inattendu (%u).\n", rep_cg.codereq);
                }
            }
        }

        else if (strncmp(commande, "invite ", 7) == 0){
            req_inviter_t req_inv;
            req_inv.codereq = 5;
            req_inv.id_client = mon_id;
            req_inv.nb = 0;

            char *ptr = commande + 7; // On saute "invite "
            char *endptr;

            long id_groupe = strtol(ptr, &endptr, 10);

            if (ptr != endptr) {
                req_inv.id_groupe = (uint16_t)id_groupe;
                ptr = endptr; // decalage jusqu'à apres le numero du groupe
                
                // on lit tout les invités (max 31) jusqu'à la fin
                while (*ptr != '\0' && req_inv.nb < 31) {
                    long id_invite_lu = strtol(ptr, &endptr, 10);
                    
                    if (ptr == endptr) {
                        break;
                    }
                    
                    req_inv.id_invites[req_inv.nb] = (uint16_t)id_invite_lu;
                    req_inv.nb++;
                    ptr = endptr;
                }
            }

            if (req_inv.nb > 0) {
                uint8_t buffer_cmd[512];
                size_t taille_msg = serialize_inviter(&req_inv, buffer_cmd);

                int sock_cmd = init_client_tcp_ipv6("::1", 8080);
                if (sock_cmd >= 0) {
                    my_send(sock_cmd, buffer_cmd, taille_msg);
                    printf("requete d'invitation envoyé au serveur\n");
                    
                    uint8_t buffer_rep[512];
                    my_recv(sock_cmd, buffer_rep, sizeof(buffer_rep));
                    close(sock_cmd); 
                    
                    printf("%d invitation(s) envoyée(s) pour le groupe %u !\n", req_inv.nb, req_inv.id_groupe);
                } else {
                    printf("Erreur de connexion au serveur.\n");
                }
            } else {
                printf("Erreur de format. Usage: /invite <id_groupe> <id_dest1> [id_dest2] [id_dest3]...\n");
            }


        }

        else{
            if (strlen(commande) > 0) printf("Commande inconnue.\n");
        }
    }


    return 0;
}