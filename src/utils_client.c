#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#include "utils.h"
#include "utils_client.h"
#include "serialisation_client.h"

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
        
        if (nom_groupe != NULL) {
            printf("Écoute Multicast activée pour le groupe \"%s\" sur le port %u.\n", nom_groupe, port);
        } else {
            printf("Écoute Multicast activée pour ce groupe sur le port %u.\n", port);
        }
    } else {
        printf("Erreur lors de la création de l'écoute Multicast.\n");
    }
}

void cmd_create(char *nom_groupe, uint16_t mon_id) {
    req_creer_groupe_t req_cg = { .codereq = 3, .id = mon_id };
    
    size_t taille_reelle = strlen(nom_groupe);
    if (taille_reelle > 255) {
        req_cg.len = 255;
    } else {
        req_cg.len = (uint8_t)taille_reelle;
    }
    strncpy(req_cg.nomg, nom_groupe, req_cg.len);

    uint8_t buffer_cmd[512], buffer_rep[512];
    size_t taille_msg = serialize_creer_groupe(&req_cg, buffer_cmd);

    printf("Requete de creation du groupe \"%s\" envoyé.\n", nom_groupe);

    if (envoyer_requete_tcp(buffer_cmd, taille_msg, buffer_rep, sizeof(buffer_rep), RESP_CREER_GROUPE_SIZE) > 0) {
        rep_creer_groupe_t rep_cg;
        deserialize_creer_groupe_resp(buffer_rep, &rep_cg);

        if (rep_cg.codereq == 4) {
            char ip[INET6_ADDRSTRLEN]; // Transforme l'IP binaire en texte
            inet_ntop(AF_INET6, rep_cg.ipmdiff, ip, INET6_ADDRSTRLEN);

            printf("\nGROUPE \"%s\" CRÉÉ AVEC SUCCÈS !\n", nom_groupe);
            printf("ID du groupe : %u\n", rep_cg.idg);
            printf("IP Multicast : %s\n", ip);
            printf("Port Multicast : %u\n\n", rep_cg.portmdiff);

            abonner_groupe_multicast(rep_cg.portmdiff, ip, nom_groupe);
        } else {
            printf("Erreur : Le serveur a renvoyé un code inattendu (%u).\n", rep_cg.codereq);
        }
    }
}

void cmd_invite(char *args, uint16_t mon_id) {
    req_inviter_t req_inv = { .codereq = 5, .id_client = mon_id, .nb = 0 };
    char *ptr = args, *endptr;

    long id_groupe = strtol(ptr, &endptr, 10);
    if (ptr == endptr) {
        printf("Erreur de format. Usage: invite <id_groupe> <id_dest1> [id_dest2]...\n");
        return;
    }

    req_inv.id_groupe = (uint16_t)id_groupe;
    ptr = endptr; // decalage jusqu'à apres le numero du groupe
    
    // on lit tout les invités (max 31) jusqu'à la fin
    while (*ptr != '\0' && req_inv.nb < 31) {
        long id_invite = strtol(ptr, &endptr, 10);
        if (ptr == endptr) break;
        req_inv.id_invites[req_inv.nb++] = (uint16_t)id_invite;
        ptr = endptr;
    }

    if (req_inv.nb > 0) {
        uint8_t buffer_cmd[512], buffer_rep[512];
        size_t taille_msg = serialize_inviter(&req_inv, buffer_cmd);

        printf("Requête d'invitation envoyée au serveur.\n");

        if (envoyer_requete_tcp(buffer_cmd, taille_msg, buffer_rep, sizeof(buffer_rep), 2) == 2) {
            rep_inviter_t rep_inv_resp;
            deserialize_inviter_resp(buffer_rep, &rep_inv_resp);
            
            if (rep_inv_resp.codereq == 24) {
                printf("%d invitation(s) envoyée(s) pour le groupe %u !\n", req_inv.nb, req_inv.id_groupe);
            } else {
                printf("Erreur : Le serveur a répondu avec le code %u (attendu: 24).\n", rep_inv_resp.codereq);
            }
        }
    }
}

void cmd_li(uint16_t mon_id) {
    req_lister_invites_t req_li = { .codereq = 6, .id_client = mon_id };
    uint8_t buffer_cmd[512], buffer_rep[512];
    size_t taille_msg = serialize_lister_invites(&req_li, buffer_cmd);

    int recu = envoyer_requete_tcp(buffer_cmd, taille_msg, buffer_rep, sizeof(buffer_rep), 0);
    if (recu >= 2) {
        rep_lister_invites_t rep_li;
        deserialize_lister_invites_resp(buffer_rep, &rep_li);

        if (rep_li.codereq == 7) {
            printf("\n--- VOS INVITATIONS EN ATTENTE (%d) ---\n", rep_li.nb);

            for (int i = 0; i < rep_li.nb; i++) {
                printf("Groupe ID : %u\n", rep_li.id_groupes[i]);
            }

            if (rep_li.nb == 0) printf("Vous n'avez aucune invitation.\n");
            printf("--------------------------------------\n\n");
        } else {
            printf("Erreur serveur (Code: %u)\n", rep_li.codereq);
        }
    }
}

void cmd_join(char *id_groupe_str, uint16_t mon_id) {
    req_action_invitation_t req_act = { .codereq = 8, .id_client = mon_id, .an = 1 };
    req_act.id_groupe = (uint16_t)atoi(id_groupe_str);

    uint8_t buffer_cmd[512], buffer_rep[2048];
    size_t taille_msg = serialize_action_invitation(&req_act, buffer_cmd);

    int recu = envoyer_requete_tcp(buffer_cmd, taille_msg, buffer_rep, sizeof(buffer_rep), 0);
    if (recu >= 22) {
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

            abonner_groupe_multicast(rep_join.portmdiff, ip_str, NULL);
        } else {
            printf("Erreur : Le serveur a répondu avec un code inattendu (%u).\n", rep_join.codereq);
        }
    } else if (recu > 0) {
        printf("Erreur : Le serveur n'a pas répondu ou réponse incomplète.\n");
    }
}

void cmd_decline(char *id_groupe_str, uint16_t mon_id) {
    req_action_invitation_t req_act = { .codereq = 8, .id_client = mon_id, .an = 0 };
    req_act.id_groupe = (uint16_t)atoi(id_groupe_str);

    uint8_t buffer_cmd[512], buffer_rep[512];
    size_t taille_msg = serialize_action_invitation(&req_act, buffer_cmd);

    if (envoyer_requete_tcp(buffer_cmd, taille_msg, buffer_rep, sizeof(buffer_rep), 0) > 0) {
        printf("Invitation pour le groupe %u refusée et supprimée.\n", req_act.id_groupe);
    }
}

void cmd_members(char *id_groupe_str, uint16_t mon_id) {
    req_demander_membres_t req_mem = { .codereq = 10, .id_client = mon_id };
    req_mem.id_groupe = (uint16_t)atoi(id_groupe_str);

    uint8_t buffer_cmd[512], buffer_rep[2048];
    size_t taille_msg = serialize_demander_membres(&req_mem, buffer_cmd);

    int recu = envoyer_requete_tcp(buffer_cmd, taille_msg, buffer_rep, sizeof(buffer_rep), 0);
    if (recu >= 4) {
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
    }
}

int initialiser_profil(uint16_t *mon_id, uint16_t *mon_port_udp, char *cle_serveur) {
    //Enregistrement des infos pour la reconnexion
    FILE *fichier = fopen("mon_profil.txt", "rb");
    if (fichier != NULL) {
        rep_inscription_t profil;
        if (fread(&profil, sizeof(rep_inscription_t), 1, fichier) == 1) {
            *mon_id = profil.id; // sauvegarde ID
            *mon_port_udp = profil.portudp;
            memcpy(cle_serveur, profil.cle, 113);
            printf("[INFO] Profil local trouvé ! Reconnexion automatique...\n");
            printf("Bon retour ! Ton ID est %u et ton port UDP est %u.\n", *mon_id, *mon_port_udp);
            fclose(fichier);
            return 1;
        }
        fclose(fichier);
    }

    char pseudo[100];
    printf("Entrez votre pseudo (10 caractères max) : ");
    if (fgets(pseudo, sizeof(pseudo), stdin) == NULL || pseudo[0] == '\n') {
        printf("Erreur : Le pseudo ne peut pas être vide.\n");
        return 0;
    }
    pseudo[strcspn(pseudo, "\n")] = 0;

    req_inscription_t req;
    // Met tout a 0 donc Clé vide (sans sécurité)
    memset(&req, 0, sizeof(req));
    req.codereq = 1;
    strncpy(req.nom, pseudo, 10);

    uint8_t buffer_cmd[REQ_INSCRIPTION_SIZE], buffer_rep[RESP_INSCRIPTION_SIZE];
    serialize_inscription_req(&req, buffer_cmd);

    int sock = init_client_tcp_ipv6(ADRESSE_LOCALE, PORT_SERVEUR);
    if (sock < 0) {
        printf("Erreur de connexion au serveur.\n");
        return 0;
    } 
    
    my_send(sock, buffer_cmd, REQ_INSCRIPTION_SIZE);
    my_recv(sock, buffer_rep, RESP_INSCRIPTION_SIZE);
    close(sock);

    rep_inscription_t rep;
    deserialize_inscription_resp(buffer_rep, &rep);

    if (rep.codereq == 2) {
        printf("INSCRIPTION RÉUSSIE !\nID : %u\nPort UDP : %u\n", rep.id, rep.portudp);
        *mon_id = rep.id;
        *mon_port_udp = rep.portudp;
        memcpy(cle_serveur, rep.cle, 113);

        fichier = fopen("mon_profil.txt", "wb");
        if (fichier) {
            fwrite(&rep, sizeof(rep_inscription_t), 1, fichier);
            fclose(fichier);
            printf("[INFO] Profil sauvegardé localement !\n");
        }
        return 1;
    } else {
        printf("\n[Erreur] Le serveur a refusé l'inscription (Code retour : %u).\n", rep.codereq);
        return 0;
    }
}