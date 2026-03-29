#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "utils.h"
#include "serialisation_serveur.h" 

#define PORT_SERVEUR 8080
#define MAX_USERS 100
#define MAX_GROUPES 50
#define MAX_INVITATIONS 150

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

// Routine exécutée indépendamment par chaque thread 
void *serve(void *arg) {
    // Récupération de la socket 
    int sockclient = *((int *)arg);
    free(arg); 

    uint8_t buffer_reception[512]; 
    uint8_t buffer_envoi[512];

    // Lecture des 2 premiers octets pour isoler le CODEREQ
    if (my_recv(sockclient, buffer_reception, 2) <= 0) {
        close(sockclient);
        return NULL;
    }

    uint16_t entete;
    memcpy(&entete, buffer_reception, 2);
    entete = ntohs(entete);
    uint8_t codereq = entete >> 11;

    switch (codereq) {
        case 1: { //Inscription
            my_recv(sockclient, buffer_reception + 2, REQ_INSCRIPTION_SIZE - 2);
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
                annuaire[nb_inscrits - 1].port_udp = 5000 + nb_inscrits;
                memcpy(annuaire[nb_inscrits - 1].cle, requete.cle, 113);

                reponse.codereq = 2; 
                reponse.id = nb_inscrits;
                reponse.portudp = annuaire[nb_inscrits - 1].port_udp;
                memset(reponse.cle, 0, 113); 
            }
            pthread_mutex_unlock(&mutex_annuaire);

            if (reponse.codereq == 2) {
                serialize_inscription_resp(&reponse, buffer_envoi);
                my_send(sockclient, buffer_envoi, RESP_INSCRIPTION_SIZE);
            }
            break;
        }

        case 3: { // Création d'un groupe
            my_recv(sockclient, buffer_reception + 2, 1); 
            uint8_t len = buffer_reception[2];
            my_recv(sockclient, buffer_reception + 3, len); // Lecture du nom du groupe

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
                groupes[nb_groupes - 1].port_mdiff = 8000 + nouvel_idg;
                
                // celui qui crée le groupe en devient automatiquement membre
                groupes[nb_groupes - 1].membres[0] = requete.id;
                groupes[nb_groupes - 1].nb_membres = 1;

                // Génération de l'adresse multicast pour ce groupe
                char ip_str_temp[INET6_ADDRSTRLEN];
                generer_adresse_multicast(nouvel_idg, ip_str_temp, groupes[nb_groupes - 1].ip_mdiff);

                reponse.codereq = 4;
                reponse.idg = nouvel_idg;
                reponse.portmdiff = groupes[nb_groupes - 1].port_mdiff;
                memcpy(reponse.ipmdiff, groupes[nb_groupes - 1].ip_mdiff, 16);
            }
            pthread_mutex_unlock(&mutex_annuaire);

            if (reponse.codereq == 4) {
                serialize_creer_groupe_resp(&reponse, buffer_envoi);
                my_send(sockclient, buffer_envoi, 20);
            }
            break;
        }

        case 5: { // Inviter des utilisateurs
            my_recv(sockclient, buffer_reception + 2, 2); 
            uint16_t ligne2;
            memcpy(&ligne2, buffer_reception + 2, 2);
            ligne2 = ntohs(ligne2);
            uint16_t idg = ligne2 >> 5;           // ID du groupe 
            uint8_t nb_invites = ligne2 & 0x1F;   // Nombre d'invités

            my_recv(sockclient, buffer_reception + 4, nb_invites * 2);

            pthread_mutex_lock(&mutex_annuaire);
            for (int i = 0; i < nb_invites; i++) {
                if (nb_invitations < MAX_INVITATIONS) {
                    uint16_t id_invite;
                    memcpy(&id_invite, buffer_reception + 4 + (i * 2), 2);
                    id_invite = ntohs(id_invite) >> 5;

                    // Ajout dans le tableau des invitations
                    invitations[nb_invitations].id_invite = id_invite;
                    invitations[nb_invitations].id_groupe = idg;
                    nb_invitations++;
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            // Envoi de l'acquittement (CODEREQ = 24)
            uint16_t ack = htons(24 << 11);
            memcpy(buffer_envoi, &ack, 2);
            my_send(sockclient, buffer_envoi, 2);
            break;
        }

        case 6: { // Demander les invitations en attente
            uint16_t id_client = entete & 0x07FF;
            uint16_t ids_groupes_invites[MAX_GROUPES];
            uint8_t nb_invits_client = 0;

            pthread_mutex_lock(&mutex_annuaire);
            // On cherche toutes les invitations pour ce client
            for (int i = 0; i < nb_invitations; i++) {
                if (invitations[i].id_invite == id_client) {
                    ids_groupes_invites[nb_invits_client] = invitations[i].id_groupe;
                    nb_invits_client++;
                }
            }

            // Préparation de la réponse (CODEREQ = 7)
            uint16_t rep_ligne1 = htons((7 << 11) | (nb_invits_client & 0x07FF));
            memcpy(buffer_envoi, &rep_ligne1, 2);
            int offset = 2; // Curseur d'écriture dans le buffer

            for (int i = 0; i < nb_invits_client; i++) {
                uint16_t idg = ids_groupes_invites[i];
                
                // Recherche des infos du groupe
                for (int j = 0; j < nb_groupes; j++) {
                    if (groupes[j].id_groupe == idg) {
                        uint8_t len = strlen(groupes[j].nom);
                        uint16_t ligne_g = htons((idg << 5) | (len & 0x1F));
                        memcpy(buffer_envoi + offset, &ligne_g, 2);
                        offset += 2;

                        // Nom du groupe
                        memcpy(buffer_envoi + offset, groupes[j].nom, len);
                        offset += len;

                        // Recherche du pseudo de l'admin du groupe
                        char pseudo_admin[10] = {0};
                        for (int k = 0; k < nb_inscrits; k++) {
                            if (annuaire[k].id == groupes[j].id_admin) {
                                memcpy(pseudo_admin, annuaire[k].pseudo, 10);
                                break;
                            }
                        }
                        memcpy(buffer_envoi + offset, pseudo_admin, 10);
                        offset += 10;
                        break; // On a trouvé le groupe, on passe au suivant
                    }
                }
            }
            pthread_mutex_unlock(&mutex_annuaire);

            my_send(sockclient, buffer_envoi, offset); 
            break;
        }

        default:
            printf("Requête non reconnue (CodeReq: %d)\n", codereq);
            break;
    }

    // on ferme la connexion TCP dès l'échange terminé
    close(sockclient);
    return NULL;
}

int main(void) {
    // Initialisation du serveur TCP IPv6
    int sock_serveur = init_serveur_tcp_ipv6(PORT_SERVEUR);
    if (sock_serveur < 0) return EXIT_FAILURE;

    printf("Serveur Paroles en écoute sur le port %d...\n", PORT_SERVEUR);

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
    }

    close(sock_serveur);
    pthread_mutex_destroy(&mutex_annuaire);
    return 0;
    
}