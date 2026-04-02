#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "utils.h"
#include <unistd.h>
#include <pthread.h>
#include "serialisation_client.h" 
#include "utils_client.h" 

#define ADRESSE_LOCALE "::1"
#define PORT_SERVEUR 8080

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

    uint16_t mon_id = 0;
    uint16_t mon_port_udp = 0;
    char cle_serveur[113];
    int deja_inscrit = 0;

    printf("BIENVENUE SUR PAROLES\n");

    FILE *fichier = fopen("mon_profil.txt", "rb");

    if (fichier != NULL) {
        rep_inscription_t profil_sauvegarde;
        if (fread(&profil_sauvegarde, sizeof(rep_inscription_t), 1, fichier) == 1) {
            mon_id = profil_sauvegarde.id;
            mon_port_udp = profil_sauvegarde.portudp;
            memcpy(cle_serveur, profil_sauvegarde.cle, 113);

            printf("[INFO] Profil local trouvé ! Reconnexion automatique...\n");
            printf("Bon retour ! Ton ID est %u et ton port UDP est %u.\n", mon_id, mon_port_udp);
            deja_inscrit = 1; 
        }
        fclose(fichier);
    }


    if (!deja_inscrit){

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
        
        int sock = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
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

            mon_id = reponse.id; // sauvegarde ID
            mon_port_udp = reponse.portudp;
            memcpy(cle_serveur, reponse.cle, 113);

            rep_inscription_t nouveau_profil;
            nouveau_profil.codereq = reponse.codereq;
            nouveau_profil.id = reponse.id;
            nouveau_profil.portudp = reponse.portudp;
            memcpy(nouveau_profil.cle, cle_serveur, 113);


            //Enregistrement des infos pour la reconnexion
            FILE *fichier_ecriture = fopen("mon_profil.txt", "wb"); 
            if (fichier_ecriture != NULL) {
                fwrite(&nouveau_profil, sizeof(rep_inscription_t), 1, fichier_ecriture);
                fclose(fichier_ecriture);
                printf("[INFO] Profil sauvegardé localement !\n");
            }

        } else {
            printf("\n[Erreur] Le serveur a refusé l'inscription (Code retour : %u).\n", reponse.codereq);
            return 1;
        }
    }

    char commande[256];

    int sock_udp = init_ecoute_udp_ipv6(mon_port_udp);
    if (sock_udp >= 0) {
        pthread_t thread_udp;
        pthread_create(&thread_udp, NULL, ecoute_notifications_udp, &sock_udp);
        printf("Écoute des notifications UDP activée sur le port %u.\n", mon_port_udp); //printf de test à enlever quand les notifs seront tous bien init
    }

    printf("\n=== MENU ===\n");
    printf("create <nom_groupe>\n");
    printf("invite <id_groupe> <id_dest1> [id_dest2] [id_dest3]...\n");
    printf("li (lister les invités)\n"); // lister les invités
    printf("join <id_groupe>\n");
    printf("decline <id_groupe>\n");
    printf("members <id_groupe> (0 pour tous)\n");
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
            size_t taille_reelle = strlen(nom_groupe);
            
            if (taille_reelle > 255) {
                req_cg.len = 255;
            } else {
                req_cg.len = (uint8_t)taille_reelle;
            }
            strncpy(req_cg.nomg, nom_groupe, req_cg.len);

            uint8_t buffer_cmd[512];
            size_t taille_msg = serialize_creer_groupe(&req_cg, buffer_cmd);

            int sock_cmd = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
            if (sock_cmd < 0) continue;

            my_send(sock_cmd, buffer_cmd, taille_msg);
            printf("Requete de creation du groupe \"%s\" envoyé.\n", nom_groupe);
            
            uint8_t buffer_rep[512];
            int recu = my_recv(sock_cmd, buffer_rep,RESP_CREER_GROUPE_SIZE );
            close(sock_cmd);

            if (recu > 0) {
                if (check_erreur_serveur(buffer_rep, recu)) {
                    continue;
                }
                rep_creer_groupe_t rep_cg;
                deserialize_creer_groupe_resp(buffer_rep, &rep_cg);

                if (rep_cg.codereq == 4) {
                    // Transforme l'IP binaire en texte
                    char ip[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, rep_cg.ipmdiff, ip, INET6_ADDRSTRLEN);

                    printf("\nGROUPE \"%s\" CRÉÉ AVEC SUCCÈS !\n", nom_groupe);
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

                int sock_cmd = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
                if (sock_cmd >= 0) {
                    my_send(sock_cmd, buffer_cmd, taille_msg);
                    printf("Requête d'invitation envoyée au serveur.\n");
                    
                    uint8_t buffer_rep[512];
                    int recu = my_recv(sock_cmd, buffer_rep, 2);
                    close(sock_cmd); 
                    
                    if (recu == 2) {
                        if (check_erreur_serveur(buffer_rep, recu)) {
                            continue;
                        }
                        rep_inviter_t rep_inv_resp;
                        deserialize_inviter_resp(buffer_rep, &rep_inv_resp);
                        
                        if (rep_inv_resp.codereq == 24) {
                            printf("%d invitation(s) envoyée(s) pour le groupe %u !\n", req_inv.nb, req_inv.id_groupe);
                        } else {
                            printf("Erreur : Le serveur a répondu avec le code %u (attendu: 24).\n", rep_inv_resp.codereq);
                        }
                    } else {
                        printf("Erreur : Le serveur a fermé la connexion avant de confirmer.\n");
                    }
                } else {
                    printf("Erreur de connexion au serveur.\n");
                }
            } else {
                printf("Erreur de format. Usage: invite <id_groupe> <id_dest1> [id_dest2] [id_dest3]...\n");
            }
        }

        else if (strcmp(commande, "li") == 0) {
            req_lister_invites_t req_li;
            req_li.codereq = 6;
            req_li.id_client = mon_id;

            uint8_t buffer_cmd[512];
            size_t taille_msg = serialize_lister_invites(&req_li, buffer_cmd);

            int sock_cmd = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
            if (sock_cmd >= 0) {
                my_send(sock_cmd, buffer_cmd, taille_msg);
                
                uint8_t buffer_rep[512];
                // recv car la taille est variable
                int recu = recv(sock_cmd, buffer_rep, sizeof(buffer_rep), 0);
                close(sock_cmd);

                if (recu >= 2) {
                    if (check_erreur_serveur(buffer_rep, recu)) {
                        continue;
                    }
                    rep_lister_invites_t rep_li;
                    deserialize_lister_invites_resp(buffer_rep, &rep_li);

                    if (rep_li.codereq == 7) {
                        printf("\n--- VOS INVITATIONS EN ATTENTE (%d) ---\n", rep_li.nb);
                        if (rep_li.nb == 0) {
                            printf("Vous n'avez aucune invitation.\n");
                        } else {
                            for (int i = 0; i < rep_li.nb; i++) {
                                printf("Groupe ID : %u\n", rep_li.id_groupes[i]);
                            }
                        }
                        printf("--------------------------------------\n\n");
                    } else {
                        printf("Erreur serveur (Code: %u)\n", rep_li.codereq);
                    }
                } else {
                    printf("Erreur de réception ou serveur déconnecté.\n");
                }
            } else {
                printf("Erreur de connexion au serveur.\n");
            }
        }

        else if (strncmp(commande, "join ", 5) == 0) {
            req_action_invitation_t req_act;
            req_act.codereq = 8;
            req_act.id_client = mon_id;
            
            req_act.id_groupe = (uint16_t)atoi(commande + 5); 
            req_act.an = 1;

            uint8_t buffer_cmd[512];
            size_t taille_msg = serialize_action_invitation(&req_act, buffer_cmd);

            int sock_cmd = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
            if (sock_cmd >= 0) {
                my_send(sock_cmd, buffer_cmd, taille_msg);
                
                uint8_t buffer_rep[2048]; 
                int recu = recv(sock_cmd, buffer_rep, sizeof(buffer_rep), 0);
                close(sock_cmd);

                if (recu >= 22) { 
                    if (check_erreur_serveur(buffer_rep, recu)) {
                        continue;
                    }

                    rep_action_invitation_acceptee_t rep_join;
                    memset(&rep_join, 0, sizeof(rep_join));
                    deserialize_action_invitation_acceptee_resp(buffer_rep, &rep_join);
                    
                    if (rep_join.codereq == 9) {
                        char ip_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, rep_join.ipmdiff, ip_str, INET6_ADDRSTRLEN);
                        
                        printf("\nINVITATION ACCEPTÉE POUR LE GROUPE %u !\n", rep_join.idg);
                        printf("IP Multicast : %s\n", ip_str);
                        printf("Port Multicast : %u\n", rep_join.portmdiff);
                        
                        printf("\n--- MEMBRES PRÉSENTS (%u) ---\n", rep_join.nb);
                        for (int i = 0; i < rep_join.nb; i++) {
                            if (i == 0) {
                                printf(" -> [%u] %s (Admin)\n", rep_join.id_membres[i], rep_join.nom_membres[i]);
                            } else {
                                printf(" -> [%u] %s\n", rep_join.id_membres[i], rep_join.nom_membres[i]);
                            }
                        }
                        printf("-----------------------------\n\n");
                        
                    } else {
                        printf("Erreur : Le serveur a répondu avec un code inattendu (%u).\n", rep_join.codereq);
                    }
                } else {
                    printf("Erreur : Le serveur n'a pas répondu ou réponse incomplète.\n");
                }
            } else {
                printf("Erreur de connexion au serveur.\n");
            }
        }

        else if (strncmp(commande, "decline ", 8) == 0) {
            req_action_invitation_t req_act;
            req_act.codereq = 8;
            req_act.id_client = mon_id;
            
            req_act.id_groupe = (uint16_t)atoi(commande + 8); 
            req_act.an = 0;

            uint8_t buffer_cmd[512];
            size_t taille_msg = serialize_action_invitation(&req_act, buffer_cmd);

            int sock_cmd = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
            if (sock_cmd >= 0) {
                my_send(sock_cmd, buffer_cmd, taille_msg);
                
                uint8_t buffer_rep[512];
                int recu = recv(sock_cmd, buffer_rep, sizeof(buffer_rep), 0);
                close(sock_cmd);

                if (recu > 0) {
                    if (check_erreur_serveur(buffer_rep, recu)) {
                        continue;
                    }
                    printf("Invitation pour le groupe %u refusée et supprimée.\n", req_act.id_groupe);
                }
            } else {
                printf("Erreur de connexion au serveur.\n");
            }
        }

        else if (strncmp(commande, "members ", 8) == 0) {
            req_demander_membres_t req_mem;
            req_mem.codereq = 10;
            req_mem.id_client = mon_id;
            
            req_mem.id_groupe = (uint16_t)atoi(commande + 8);

            uint8_t buffer_cmd[512];
            size_t taille_msg = serialize_demander_membres(&req_mem, buffer_cmd);

            int sock_cmd = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
            if (sock_cmd >= 0) {
                my_send(sock_cmd, buffer_cmd, taille_msg);
                
                uint8_t buffer_rep[2048]; 
                int recu = recv(sock_cmd, buffer_rep, sizeof(buffer_rep), 0);
                close(sock_cmd);

                if (recu >= 4) {
                    if (check_erreur_serveur(buffer_rep, recu)) {
                        continue;
                    }
                    rep_demander_membres_t rep_mem;
                    deserialize_demander_membres_resp(buffer_rep, &rep_mem);

                    if (rep_mem.codereq == 11) {
                        
                        if (req_mem.id_groupe == 0) {
                            printf("\n--- LISTE DE TOUS LES UTILISATEURS (%u) ---\n", rep_mem.nb);
                        } else {
                            printf("\n--- MEMBRES DU GROUPE %u (%u) ---\n", rep_mem.idg, rep_mem.nb);
                        }
                        
                        if (rep_mem.nb == 0) {
                            printf("Aucun membre trouvé.\n");
                        } else {
                            for (int i = 0; i < rep_mem.nb; i++) {
                                // premier membre = admin du groupe
                                if (i == 0 && req_mem.id_groupe != 0) {
                                    printf(" -> [%u] %s (Admin)\n", rep_mem.id_membres[i], rep_mem.nom_membres[i]);
                                } else {
                                    printf(" -> [%u] %s\n", rep_mem.id_membres[i], rep_mem.nom_membres[i]);
                                }
                            }
                        }
                        printf("-------------------------------------------\n\n");
                        
                    } else {
                        printf("Erreur : Le serveur a renvoyé un code inattendu (%u).\n", rep_mem.codereq);
                    }
                } else {
                    printf("Le serveur n'a pas répondu correctement ou a fermé la connexion.\n");
                }
            } else {
                printf("Erreur de connexion au serveur.\n");
            }
        }

        else{
            if (strlen(commande) > 0) printf("Commande inconnue.\n");
        }
    }


    return 0;
}