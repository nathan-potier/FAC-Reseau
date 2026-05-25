#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <signal.h>

#include "utils.h"
#include "serialisation_serveur.h" 

#define PORT_SERVEUR 8080
#define MAX_USERS 100
#define MAX_GROUPES 50
#define MAX_INVITATIONS 150
#define ADRESSE_LOCALE "::1"
#define MAX_MESSAGES 2000

SSL_CTX *ctx_global;
char cle_publique_serveur[113];

// Représentation interne d'un utilisateur côté serveur
typedef struct {
    uint16_t id;
    char pseudo[11];
    uint16_t port_udp;
    char cle[113];
} utilisateur_t;

// Annuaire stockant les clients inscrits
utilisateur_t annuaire[MAX_USERS];
uint16_t nb_inscrits = 0;

// Structure d'un groupe côté serveur
typedef struct {
    uint16_t id_groupe;
    char nom[256];
    uint16_t id_admin;
    uint16_t port_mdiff;
    uint8_t ip_mdiff[16];
    uint16_t membres[MAX_USERS];
    uint16_t nb_membres;
} groupe_t;

// Base de données des groupes
groupe_t groupes[MAX_GROUPES];
uint16_t nb_groupes = 0;

// Structure d'une invitation en attente
typedef struct {
    uint16_t id_invite;
    uint16_t id_groupe;
} invitation_t;

// Base de données des invitations
invitation_t invitations[MAX_INVITATIONS];
uint16_t nb_invitations = 0;

//Mutex empêchant plusieurs threads de modifier l'annuaire simultanément 
pthread_mutex_t mutex_annuaire = PTHREAD_MUTEX_INITIALIZER;

// struct message
typedef struct {
    uint16_t id_groupe;    // groupe
    uint16_t id_auteur;    // auteur
    uint16_t numb;         // num
    uint16_t numr;         // num rep
    uint16_t len;           // taille msg
    char contenu[2048];     // text msg
} message_t;

// BDD qui conserve l'ordre de réception
message_t historique[MAX_MESSAGES];
uint16_t nb_messages = 0;


uint16_t compteurs_numb[MAX_GROUPES]; 
uint16_t compteurs_numr[MAX_GROUPES][500];
// Mutex pour protéger l'accès concurrent aux messages
pthread_mutex_t mutex_messages = PTHREAD_MUTEX_INITIALIZER;

// Routine exécutée indépendamment par chaque thread 
void *serve(void *arg) {
    // Récupération de la socket 
    int sockclient = *((int *)arg);
    free(arg); 


    // sécurisation TLS de la connexion avec le client
    SSL *ssl = SSL_new(ctx_global);
    SSL_set_fd(ssl, sockclient);

    if (SSL_accept(ssl) <= 0) { // TLS Handshake 
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sockclient);
        return NULL;
    }


    uint8_t buffer_reception[4096]; 
    uint8_t buffer_envoi[4096];

    struct timeval tv;
    tv.tv_sec = 120; // 120 secondes d'attente max
    tv.tv_usec = 0;
    setsockopt(sockclient, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));


    // Lecture des 2 premiers octets pour isoler le CODEREQ
    if (my_recv_ssl(ssl, buffer_reception, 2) <= 0) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sockclient);
        return NULL;
    }

    uint16_t entete;
    memcpy(&entete, buffer_reception, 2);
    entete = ntohs(entete);
    uint8_t codereq = entete >> 11;
    uint16_t id_client = entete & 0x07FF;

    if (codereq == 0) {
        //Lire les 70 octets restants du message d'authentification
        if (my_recv_ssl(ssl, buffer_reception + 2, 70) <= 0) {
            SSL_shutdown(ssl); SSL_free(ssl); close(sockclient); return NULL;
        }

        // Récupérer la clé publique du client depuis l'annuaire
        char cle_client[113] = {0};
        int client_trouve = 0;
        pthread_mutex_lock(&mutex_annuaire);
        if (id_client > 0 && id_client <= nb_inscrits) {
            memcpy(cle_client, annuaire[id_client - 1].cle, 113);
            client_trouve = 1;
        }
        pthread_mutex_unlock(&mutex_annuaire);

        if (!client_trouve) {
            envoyer_erreur_tcp_ssl(ssl, "Erreur : Client inconnu.");
            SSL_shutdown(ssl); SSL_free(ssl); close(sockclient); return NULL;
        }

        // Vérifier la signature du client
        if (!verifier_signature_ed25519(cle_client, buffer_reception, 6, buffer_reception + 8)) {
            printf("[Alerte Serveur] Usurpation d'identité bloquée pour l'ID %u !\n", id_client);
            SSL_shutdown(ssl); SSL_free(ssl); close(sockclient); return NULL;
        }

        // Réponse du serveur
        uint8_t buffer_auth_rep[72];
        memcpy(buffer_auth_rep, buffer_reception, 6); // On copie CODEREQ(0), ID et NUM
        uint16_t lsig_net = htons(64);
        memcpy(buffer_auth_rep + 6, &lsig_net, 2);

        //Le serveur signe les 6 octets
        uint8_t signature_serveur[64];
        if (!signer_message_ed25519(buffer_auth_rep, 6, signature_serveur, "priv_serveur.pem")) {
            SSL_shutdown(ssl); SSL_free(ssl); close(sockclient); return NULL;
        }
        memcpy(buffer_auth_rep + 8, signature_serveur, 64);

        // Renvoyer la validation au client
        my_send_ssl(ssl, buffer_auth_rep, 72);

        if (my_recv_ssl(ssl, buffer_reception, 2) <= 0) {
            SSL_shutdown(ssl); SSL_free(ssl); close(sockclient); return NULL;
        }
        memcpy(&entete, buffer_reception, 2);
        entete = ntohs(entete);
        codereq = entete >> 11; // On met à jour le codereq
    }


    //A FACTORISER QUAND ON A LE TEMPS FONCTIONS POUR CHAQUE CODEREQ 
    switch (codereq) {
        case 1: { //Inscription
            my_recv_ssl(ssl,  buffer_reception + 2, REQ_INSCRIPTION_SIZE - 2);
            req_inscription_t requete;
            deserialize_inscription_req(buffer_reception, &requete);

            rep_inscription_t reponse;
            memset(&reponse, 0, sizeof(reponse));

            //on verrouille l'accès à l'annuaire
            pthread_mutex_lock(&mutex_annuaire);
            if (nb_inscrits < MAX_USERS) {
                nb_inscrits++;
                annuaire[nb_inscrits - 1].id = nb_inscrits;
                strncpy(annuaire[nb_inscrits - 1].pseudo, requete.nom, 10);
                annuaire[nb_inscrits - 1].pseudo[10] = '\0';
                annuaire[nb_inscrits - 1].port_udp = 5000 + nb_inscrits;
                memcpy(annuaire[nb_inscrits - 1].cle, requete.cle, 113);

                reponse.codereq = 2; 
                reponse.id = nb_inscrits;
                reponse.portudp = annuaire[nb_inscrits - 1].port_udp;
                memcpy(reponse.cle, cle_publique_serveur, 113);
            }
            pthread_mutex_unlock(&mutex_annuaire);

            if (reponse.codereq == 2) {
                serialize_inscription_resp(&reponse, buffer_envoi);
                my_send_ssl(ssl, buffer_envoi, RESP_INSCRIPTION_SIZE);
            }
            break;
        }

        case 3: { // Création d'un groupe
            my_recv_ssl(ssl,  buffer_reception + 2, 1); 
            uint8_t len = buffer_reception[2];
            my_recv_ssl(ssl,  buffer_reception + 3, len); // Lecture du nom du groupe

            req_creer_groupe_t requete;
            deserialize_creer_groupe_req(buffer_reception, &requete);

            rep_creer_groupe_t reponse;
            memset(&reponse, 0, sizeof(reponse));

            pthread_mutex_lock(&mutex_annuaire); 
            if (nb_groupes < MAX_GROUPES) {
                nb_groupes++;
                uint16_t nouvel_idg = nb_groupes;

                groupes[nb_groupes - 1].id_groupe = nouvel_idg;
                groupes[nb_groupes - 1].id_admin = requete.id;
                strncpy(groupes[nb_groupes - 1].nom, requete.nomg, len);
                groupes[nb_groupes - 1].nom[len] = '\0';
                groupes[nb_groupes - 1].port_mdiff = 8000 + nouvel_idg;
                
                // celui qui crée le groupe en devient automatiquement membre
                groupes[nb_groupes - 1].membres[0] = requete.id;
                groupes[nb_groupes - 1].nb_membres = 1;

                // Génération de l'adresse multicast pour ce groupe
                char ip_str_temp[INET6_ADDRSTRLEN];
                generer_adresse_multicast(nouvel_idg, ip_str_temp, groupes[nb_groupes - 1].ip_mdiff);

                reponse.codereq = 4;
                reponse.id_groupe = nouvel_idg;
                reponse.portmdiff = groupes[nb_groupes - 1].port_mdiff;
                memcpy(reponse.ipmdiff, groupes[nb_groupes - 1].ip_mdiff, 16);
            }
            pthread_mutex_unlock(&mutex_annuaire);

            if (reponse.codereq == 4) {
                serialize_creer_groupe_resp(&reponse, buffer_envoi);
                my_send_ssl(ssl, buffer_envoi, 20);
            }
            break;
        }

        case 5: { // Inviter des utilisateurs
            my_recv_ssl(ssl,  buffer_reception + 2, 2); 
            uint16_t ligne2;
            memcpy(&ligne2, buffer_reception + 2, 2);
            uint8_t nb_invites = ntohs(ligne2) & 0x1F;
            my_recv_ssl(ssl,  buffer_reception + 4, nb_invites * 2);

            req_inviter_t req;
            deserialize_inviter_req(buffer_reception, &req);

            pthread_mutex_lock(&mutex_annuaire);
            int idx_groupe = req.id_groupe - 1;
            if (req.id_groupe == 0 || req.id_groupe > nb_groupes || groupes[idx_groupe].id_admin != req.id_client) {
                pthread_mutex_unlock(&mutex_annuaire);
                envoyer_erreur_tcp_ssl(ssl, "Erreur : Seul l'administrateur peut inviter des membres.");
                break;
            }

            for (int i = 0; i < req.nb; i++) {
                if (nb_invitations < MAX_INVITATIONS) {
                    // Ajout dans le tableau des invitations
                    invitations[nb_invitations].id_invite = req.id_invites[i];
                    invitations[nb_invitations].id_groupe = req.id_groupe;
                    nb_invitations++;

                    if (req.id_invites[i] > 0 && req.id_invites[i] <= nb_inscrits) {
                        int index_client = req.id_invites[i] - 1; 
                        // CODEREQ(22) sur 5 bits + IDG sur 11 bits
                        uint16_t notif_invit = htons((22 << 11) | (req.id_groupe & 0x07FF));
                        // paquet UDP vers le client 
                        envoyer_message_udp_ipv6(ADRESSE_LOCALE, annuaire[index_client].port_udp, (const uint8_t *)&notif_invit, 2); // vérifier si on envoie a la bonne personne 
                    }
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            uint16_t ack = htons(24 << 11);
            memcpy(buffer_envoi, &ack, 2);
            my_send_ssl(ssl, buffer_envoi, 2);
            break;
        }

        case 6: { // Demander les invitations en attente
            req_lister_invites_t req;
            deserialize_lister_invites_req(buffer_reception, &req);

            rep_lister_invites_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.codereq = 7;
            rep.nb = 0;

            pthread_mutex_lock(&mutex_annuaire);
            for (int i = 0; i < nb_invitations; i++) {
                if (invitations[i].id_invite == req.id_client) {
                    uint16_t idg = invitations[i].id_groupe;
                    rep.id_groupes[rep.nb] = idg;
                    
                    // Recherche des informations complémentaires du groupe
                    if (idg > 0 && idg <= nb_groupes) {
                        int idx_groupe = idg - 1;
                        
                        // Nom du groupe et sa taille
                        size_t len = strlen(groupes[idx_groupe].nom);
                        if (len > 31) len = 31; // taille sur 5 bits = max 31
                        rep.len_groupes[rep.nb] = (uint8_t)len;
                        strncpy(rep.nom_groupes[rep.nb], groupes[idx_groupe].nom, len);
                        
                        // Pseudo de l'admin
                        uint16_t id_admin = groupes[idx_groupe].id_admin;
                        if (id_admin > 0 && id_admin <= nb_inscrits) {
                            strncpy(rep.nom_admins[rep.nb], annuaire[id_admin - 1].pseudo, 10);
                        } else {
                            strncpy(rep.nom_admins[rep.nb], "Inconnu", 10);
                        }
                    }
                    
                    rep.nb++;
                    if (rep.nb >= 31) break; // Sécurité pour ne pas dépasser la taille du tableau (31 max)
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            size_t taille = serialize_lister_invites_resp(&rep, buffer_envoi);
            my_send_ssl(ssl, buffer_envoi, taille);
            break;
        }

        case 8: { // Répondre à une invitation (Accepter ou Refuser)
            my_recv_ssl(ssl,  buffer_reception + 2, 2); 
            req_action_invitation_t req;
            deserialize_action_invitation_req(buffer_reception, &req);
            pthread_mutex_lock(&mutex_annuaire);

            // Supprimer l'invitation en attente
            int est_invite = 0;
            for (int i = 0; i < nb_invitations; i++) {
                if (invitations[i].id_invite == req.id_client && invitations[i].id_groupe == req.id_groupe) {
                    invitations[i] = invitations[nb_invitations - 1]; 
                    nb_invitations--;
                    est_invite = 1;
                    break;
                }
            }

            //Rejoindre sans invitation
            if (req.an == 1 && !est_invite) {
                pthread_mutex_unlock(&mutex_annuaire);
                envoyer_erreur_tcp_ssl(ssl, "Action refusee : Vous n'avez pas d'invitation.");
                break;
            }

            // Traitement selon l'action
            if (req.an == 1) { // Accepte AN = 1
                int idx_groupe = -1;
                if (req.id_groupe > 0 && req.id_groupe <= nb_groupes) {
                    idx_groupe = req.id_groupe - 1;
                }

                if (idx_groupe != -1) {
                    int deja_membre = 0;
                    for (int i = 0; i < groupes[idx_groupe].nb_membres; i++) {
                        if (groupes[idx_groupe].membres[i] == req.id_client) {
                            deja_membre = 1; break;
                        }
                    }

                    if (deja_membre) {
                        pthread_mutex_unlock(&mutex_annuaire);
                        envoyer_erreur_tcp_ssl(ssl, "Action refusee : Vous etes deja membre de ce groupe.");
                        break;
                    }

                    groupes[idx_groupe].membres[groupes[idx_groupe].nb_membres] = req.id_client;
                    groupes[idx_groupe].nb_membres++;

                    rep_action_invitation_acceptee_t rep;
                    rep.codereq = 9;
                    rep.id_groupe = req.id_groupe;
                    rep.portmdiff = groupes[idx_groupe].port_mdiff;
                    memcpy(rep.ipmdiff, groupes[idx_groupe].ip_mdiff, 16);
                    rep.nb = groupes[idx_groupe].nb_membres;

                    for (int i = 0; i < groupes[idx_groupe].nb_membres; i++) {
                        rep.id_membres[i] = groupes[idx_groupe].membres[i];
                        memset(rep.nom_membres[i], 0, 10);
                        uint16_t id_recherche = rep.id_membres[i];
                        if (id_recherche > 0 && id_recherche <= nb_inscrits) {
                            strncpy(rep.nom_membres[i], annuaire[id_recherche - 1].pseudo, 10);
                        }
                    }

                    size_t taille = serialize_accepter_invitation_resp(&rep, buffer_envoi);
                    my_send_ssl(ssl, buffer_envoi, taille);

                    char ip_str[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, groupes[idx_groupe].ip_mdiff, ip_str, INET6_ADDRSTRLEN);

                    // CODEREQ(19) + ID groupe
                    uint16_t notif_accept = htons((19 << 11) | (req.id_groupe & 0x07FF));
                    envoyer_message_udp_ipv6(ip_str, groupes[idx_groupe].port_mdiff, (const uint8_t *)&notif_accept, 2);
                }else {
                    pthread_mutex_unlock(&mutex_annuaire);
                    envoyer_erreur_tcp_ssl(ssl, "Erreur : Ce groupe n'existe pas.");
                    break;
                }   
            } else { // Refuse AN = 0 ou Quitte AN = 2
                uint16_t ack = htons(24 << 11);
                memcpy(buffer_envoi, &ack, 2);
                my_send_ssl(ssl, buffer_envoi, 2);

                if (req.an == 2) { // Si le client a quiitté le groupe (et non juste refusé l'invitation)
                    if (req.id_groupe > 0 && req.id_groupe <= nb_groupes && groupes[req.id_groupe - 1].id_groupe == req.id_groupe) {
                        int idx_groupe = req.id_groupe - 1; //faire attention a ne pas décaler dans le tableau si on en supprime 
                        char ip_str[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, groupes[idx_groupe].ip_mdiff, ip_str, INET6_ADDRSTRLEN);
                        
                        // CODEREQ(21) + IDG
                        if (req.id_client == groupes[idx_groupe].id_admin) {
                            uint16_t notif_close = htons((21 << 11) | (req.id_groupe & 0x07FF));
                            envoyer_message_udp_ipv6(ip_str, groupes[idx_groupe].port_mdiff, (const uint8_t *)&notif_close, 2);

                            // On "désactive" le groupe proprement (sans décaler le tableau, attention à ne pas changer plus tard)
                            groupes[idx_groupe].id_groupe = 0; 
                        } 
                        // CODEREQ(20) + IDG
                        else {
                            for (int k = 0; k < groupes[idx_groupe].nb_membres; k++) {
                                if (groupes[idx_groupe].membres[k] == req.id_client) {
                                    // On remplace sa case par le dernier membre du tableau pour boucher le trou
                                    groupes[idx_groupe].membres[k] = groupes[idx_groupe].membres[groupes[idx_groupe].nb_membres - 1];
                                    groupes[idx_groupe].nb_membres--;
                                    break;
                                }
                            }
                            uint16_t notif_leave = htons((20 << 11) | (req.id_groupe & 0x07FF));
                            envoyer_message_udp_ipv6(ip_str, groupes[idx_groupe].port_mdiff, (const uint8_t *)&notif_leave, 2);
                        }
                    }
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);
            break;
        }

        case 10: { // Demander la liste des membres d'un groupe ou de tous les utilisateurs
            my_recv_ssl(ssl,  buffer_reception + 2, 2); 
            req_demander_membres_t req;
            deserialize_lister_membres_req(buffer_reception, &req);

            //vérif est membre du gorupe
            pthread_mutex_lock(&mutex_annuaire);
            int est_membre = 0;
            if (req.id_groupe == 0) {
                est_membre = 1; // L'annuaire global est public
            } else {
                int idx_groupe = req.id_groupe - 1;
                if (req.id_groupe > 0 && req.id_groupe <= nb_groupes && groupes[idx_groupe].id_groupe == req.id_groupe) {
                    for (int i = 0; i < groupes[idx_groupe].nb_membres; i++) {
                        if (groupes[idx_groupe].membres[i] == req.id_client) {
                            est_membre = 1; break;
                        }
                    }
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            if (!est_membre) {
                envoyer_erreur_tcp_ssl(ssl, "Erreur : Vous devez etre membre du groupe pour poster.");
                break;
            }

            rep_demander_membres_t rep;
            rep.codereq = 11;
            rep.id_groupe = req.id_groupe;
            rep.nb = 0;

            pthread_mutex_lock(&mutex_annuaire);

            if (req.id_groupe == 0) { // Tous les utilisateurs inscrits
                rep.nb = nb_inscrits;
                for (int i = 0; i < nb_inscrits; i++) {
                    rep.id_membres[i] = annuaire[i].id;
                    strncpy(rep.nom_membres[i], annuaire[i].pseudo, 10);
                }
            } else { // Membres d'un groupe spécifique
                int idx_groupe = -1;
                for (int i = 0; i < nb_groupes; i++) {
                    if (groupes[i].id_groupe == req.id_groupe) {
                        idx_groupe = i;
                        break;
                    }
                }
                if (idx_groupe != -1) {
                    rep.nb = groupes[idx_groupe].nb_membres;
                    for (int i = 0; i < groupes[idx_groupe].nb_membres; i++) {
                        rep.id_membres[i] = groupes[idx_groupe].membres[i];
                        memset(rep.nom_membres[i], 0, 10);
                        // Cherche le pseudo correspondant à l'ID
                        uint16_t id_recherche = rep.id_membres[i];
                        if (id_recherche > 0 && id_recherche <= nb_inscrits) {
                            strncpy(rep.nom_membres[i], annuaire[id_recherche - 1].pseudo, 10);
                        }
                    }
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            size_t taille = serialize_lister_membres_resp(&rep, buffer_envoi);
            my_send_ssl(ssl, buffer_envoi, taille);
            break;
        }

        case 12: { // Poster un nouveau billet
            my_recv_ssl(ssl,  buffer_reception + 2, 4); 
            uint16_t len_net;
            memcpy(&len_net, buffer_reception + 4, 2);
            uint16_t len = ntohs(len_net);
            
            my_recv_ssl(ssl,  buffer_reception + 6, len);
            req_poster_billet_t req;
            deserialize_poster_billet_req(buffer_reception, &req);

            //vérif est membre du gorupe
            pthread_mutex_lock(&mutex_annuaire);
            int est_membre = 0;
            int idx_groupe = req.id_groupe - 1;
            if (req.id_groupe > 0 && req.id_groupe <= nb_groupes && groupes[idx_groupe].id_groupe == req.id_groupe) {
                for (int i = 0; i < groupes[idx_groupe].nb_membres; i++) {
                    if (groupes[idx_groupe].membres[i] == req.id_client) {
                        est_membre = 1; break;
                    }
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            if (!est_membre) {
                envoyer_erreur_tcp_ssl(ssl, "Erreur : Vous devez etre membre du groupe pour poster.");
                break;
            }

            rep_poster_billet_t rep;
            rep.codereq = 13;
            rep.id_groupe = req.id_groupe;

            pthread_mutex_lock(&mutex_messages);
            
            uint16_t nouveau_numb = compteurs_numb[req.id_groupe % MAX_GROUPES];
            compteurs_numb[req.id_groupe % MAX_GROUPES]++;

            // Ajout chronologique dans la base de données
            if (nb_messages < MAX_MESSAGES) {
                historique[nb_messages].id_groupe = req.id_groupe;
                historique[nb_messages].id_auteur = req.id_client;
                historique[nb_messages].numb = nouveau_numb;
                historique[nb_messages].numr = 0; 
                historique[nb_messages].len = req.len;
                memcpy(historique[nb_messages].contenu, req.data, req.len);
                nb_messages++;
            }
            pthread_mutex_unlock(&mutex_messages);
            // Envoi de la confirmation au client (CODEREQ 13)
            rep.numb = nouveau_numb;
            serialize_poster_billet_resp(&rep, buffer_envoi);
            my_send_ssl(ssl, buffer_envoi, 4); 
            
            // Notification Multicast (CODEREQ 18) pour prévenir le groupe
            pthread_mutex_lock(&mutex_annuaire);
            for (int i = 0; i < nb_groupes; i++) {
                if (groupes[i].id_groupe == req.id_groupe) {
                    char ip_str[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, groupes[i].ip_mdiff, ip_str, INET6_ADDRSTRLEN);
                    
                    uint16_t notif = htons((18 << 11) | (req.id_groupe & 0x07FF));
                    envoyer_message_udp_ipv6(ip_str, groupes[i].port_mdiff, (const uint8_t *)&notif, 2);
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);
            break;
        }

        case 14: { // Répondre à un billet existant
            my_recv_ssl(ssl,  buffer_reception + 2, 6); 
            
            uint16_t len_net;
            memcpy(&len_net, buffer_reception + 6, 2);
            uint16_t len = ntohs(len_net);
        
            my_recv_ssl(ssl,  buffer_reception + 8, len);

            req_repondre_billet_t req;
            deserialize_repondre_billet_req(buffer_reception, &req);

            //vérif est membre du gorupe
            pthread_mutex_lock(&mutex_annuaire);
            int est_membre = 0;
            int idx_groupe = req.id_groupe - 1;
            if (req.id_groupe > 0 && req.id_groupe <= nb_groupes && groupes[idx_groupe].id_groupe == req.id_groupe) {
                for (int i = 0; i < groupes[idx_groupe].nb_membres; i++) {
                    if (groupes[idx_groupe].membres[i] == req.id_client) {
                        est_membre = 1; break;
                    }
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            if (!est_membre) {
                envoyer_erreur_tcp_ssl(ssl, "Erreur : Vous devez etre membre du groupe pour poster.");
                break;
            }

            pthread_mutex_lock(&mutex_messages);
            int billet_trouve = 0;
            for (int i = 0; i < nb_messages; i++) {
                if (historique[i].id_groupe == req.id_groupe && 
                    historique[i].numb == req.numb && 
                    historique[i].numr == 0) {
                    billet_trouve = 1;
                    break;
                }
            }

            if (!billet_trouve) {
                pthread_mutex_unlock(&mutex_messages);
                envoyer_erreur_tcp_ssl(ssl, "Erreur : Le billet auquel vous repondez n'existe pas !");
                break;
            }

            pthread_mutex_unlock(&mutex_messages);

            rep_repondre_billet_t rep;
            rep.codereq = 15;
            rep.id_groupe = req.id_groupe;
            rep.numb = req.numb;

            pthread_mutex_lock(&mutex_messages);
            
            compteurs_numr[req.id_groupe % MAX_GROUPES][req.numb % 500]++;
            uint16_t nouveau_numr = compteurs_numr[req.id_groupe % MAX_GROUPES][req.numb % 500];

            if (nb_messages < MAX_MESSAGES) {
                historique[nb_messages].id_groupe = req.id_groupe;
                historique[nb_messages].id_auteur = req.id_client;
                historique[nb_messages].numb = req.numb;
                historique[nb_messages].numr = nouveau_numr; 
                historique[nb_messages].len = req.len;
                memcpy(historique[nb_messages].contenu, req.data, req.len);
                nb_messages++;
            }
            pthread_mutex_unlock(&mutex_messages);

            // Envoi de la confirmation au client (CODEREQ 15)
            rep.numr = nouveau_numr;
            serialize_repondre_billet_resp(&rep, buffer_envoi);
            my_send_ssl(ssl, buffer_envoi, 4); 

            // Notification Multicast (CODEREQ 18)
            pthread_mutex_lock(&mutex_annuaire);
            for (int i = 0; i < nb_groupes; i++) {
                if (groupes[i].id_groupe == req.id_groupe) {
                    char ip_str[INET6_ADDRSTRLEN];
                    inet_ntop(AF_INET6, groupes[i].ip_mdiff, ip_str, INET6_ADDRSTRLEN);
                    
                    uint16_t notif = htons((18 << 11) | (req.id_groupe & 0x07FF));
                    envoyer_message_udp_ipv6(ip_str, groupes[i].port_mdiff, (const uint8_t *)&notif, 2);
                    break;
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);
            break;
        }

        case 16: { // Demander l'historique
            my_recv_ssl(ssl,  buffer_reception + 2, 4); 
            req_demander_billets_t req;
            deserialize_demander_billets_req(buffer_reception, &req);

            //vérif est membre du gorupe
            pthread_mutex_lock(&mutex_annuaire);
            int est_membre = 0;
            int idx_groupe = req.id_groupe - 1;
            if (req.id_groupe > 0 && req.id_groupe <= nb_groupes && groupes[idx_groupe].id_groupe == req.id_groupe) {
                for (int i = 0; i < groupes[idx_groupe].nb_membres; i++) {
                    if (groupes[idx_groupe].membres[i] == req.id_client) {
                        est_membre = 1; break;
                    }
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            if (!est_membre) {
                envoyer_erreur_tcp_ssl(ssl, "Erreur : Vous devez etre membre du groupe pour poster.");
                break;
            }

            pthread_mutex_lock(&mutex_messages);

            int idx_start = -1;
            
            if (req.numb != 2047) {
                idx_start = nb_messages; 
                
                for (int i = 0; i < nb_messages; i++) {
                    if (historique[i].id_groupe == req.id_groupe && 
                        historique[i].numb == req.numb && 
                        historique[i].numr == req.numr) {
                        idx_start = i;
                        break;
                    }
                }
            }

            uint16_t nb_a_envoyer = 0;
            uint8_t *buffer_historique = malloc(65535); // 65535 est la limite absolue d'un paquet réseau (IP)
            if (buffer_historique == NULL) break;
            int offset = 4;

            for (int i = idx_start + 1; i < nb_messages; i++) {
                if (historique[i].id_groupe == req.id_groupe) {
                    nb_a_envoyer++;
                    
                    uint16_t l3_msg = htons(historique[i].id_auteur << 5); 
                    memcpy(buffer_historique + offset, &l3_msg, 2);
                    offset += 2;

                    // L4 : numb1 (0-10) | numr1 (11-15)
                    uint16_t l4_msg = htons((historique[i].numb << 5) | (historique[i].numr & 0x1F));
                    memcpy(buffer_historique + offset, &l4_msg, 2);
                    offset += 2;

                    // L5 : len1 (0-15)
                    uint16_t msg_len_net = htons(historique[i].len);
                    memcpy(buffer_historique + offset, &msg_len_net, 2);
                    offset += 2;

                    // L6 : data1 (0-...)
                    memcpy(buffer_historique + offset, historique[i].contenu, historique[i].len);
                    offset += historique[i].len;

                    if (historique[i].id_auteur != req.id_client) {
                        if (historique[i].id_auteur > 0 && historique[i].id_auteur <= nb_inscrits) {
                            uint16_t port_auteur = annuaire[historique[i].id_auteur - 1].port_udp;
                            uint16_t notif_read = htons((23 << 11) | (req.id_groupe & 0x07FF));
                            envoyer_message_udp_ipv6(ADRESSE_LOCALE, port_auteur, (const uint8_t *)&notif_read, 2);
                        }
                    }

                }
            }
            pthread_mutex_unlock(&mutex_messages);

            uint16_t l1 = htons((17 << 11) | (req.id_groupe & 0x07FF));
            memcpy(buffer_historique, &l1, 2);

            uint16_t nb_net = htons(nb_a_envoyer);
            memcpy(buffer_historique + 2, &nb_net, 2);

            my_send_ssl(ssl, buffer_historique, offset);
            free(buffer_historique);
            break;
        }


        default:
            printf("Requête non reconnue (CodeReq: %d)\n", codereq);
            envoyer_erreur_tcp_ssl(ssl, "Erreur : Code de requete inconnu ou mal formaté.");
            break;
    }

    // on ferme la connexion TCP dès l'échange terminé
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockclient);
    return NULL;
}

int main(void) {
    // Initialisation du serveur TCP IPv6
    int sock_serveur = init_serveur_tcp_ipv6(PORT_SERVEUR);
    if (sock_serveur < 0) return EXIT_FAILURE;

    printf("Serveur Paroles en écoute sur le port %d...\n", PORT_SERVEUR);


    struct sigaction action = {0};
    action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &action, NULL);

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    ctx_global = SSL_CTX_new(TLS_server_method());
    if (!ctx_global) { ERR_print_errors_fp(stderr); exit(1); }

    // Chargement des certificats
    if (SSL_CTX_use_certificate_file(ctx_global, "serv.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr); exit(1);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx_global, "serv.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr); exit(1);
    }

    generer_cles_ed25519(cle_publique_serveur, "serveur");

    while (1) {
        struct sockaddr_in6 adrclient;
        socklen_t size = sizeof(adrclient);
        
        int *sockclient = malloc(sizeof(int));
        if (!sockclient) continue;

        *sockclient = accept(sock_serveur, (struct sockaddr *)&adrclient, &size);
        if (*sockclient < 0) {
            free(sockclient);
            continue;
        }


        // Création du thread avec la fonction 'serve'
        pthread_t thread; 
        if (pthread_create(&thread, NULL, serve, sockclient) != 0) {
            close(*sockclient);
            free(sockclient);
        }
        else{
            pthread_detach(thread);
        }
        
    }

    close(sock_serveur);
    pthread_mutex_destroy(&mutex_annuaire);
    pthread_mutex_destroy(&mutex_messages);
    return 0;
    
}