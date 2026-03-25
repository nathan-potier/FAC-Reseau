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

//Mutex empêchant plusieurs threads de modifier l'annuaire simultanément 
pthread_mutex_t mutex_annuaire = PTHREAD_MUTEX_INITIALIZER;

// Routine exécutée indépendamment par chaque thread 
void *serve(void *arg) {
    // Récupération de la socket 
    int sockclient = *((int *)arg);
    free(arg); 

    uint8_t buffer_reception[REQ_INSCRIPTION_SIZE];
    uint8_t buffer_envoi[RESP_INSCRIPTION_SIZE];

    // Lecture sécurisée : my_recv gère la fragmentation TCP
    if (my_recv(sockclient, buffer_reception, REQ_INSCRIPTION_SIZE) <= 0) {
        close(sockclient);
        return NULL; 
    }

    req_inscription_t requete;
    deserialize_inscription_req(buffer_reception, &requete);

    // Traitement de la demande d'inscription (Code 1)
    if (requete.codereq == 1) { 
        rep_inscription_t reponse;
        memset(&reponse, 0, sizeof(reponse));

        //on verrouille l'accès à l'annuaire
        pthread_mutex_lock(&mutex_annuaire);

        if (nb_inscrits < MAX_USERS) {
            nb_inscrits++;
            uint16_t nouvel_id = nb_inscrits;
            uint16_t port_attribue = 5000 + nouvel_id; 

            annuaire[nb_inscrits - 1].id = nouvel_id;
            strncpy(annuaire[nb_inscrits - 1].pseudo, requete.nom, 10);
            annuaire[nb_inscrits - 1].port_udp = port_attribue;
            memcpy(annuaire[nb_inscrits - 1].cle, requete.cle, 113);

            // Préparation de la réponse (Code 2 = Succès)
            reponse.codereq = 2; 
            reponse.id = nouvel_id;
            reponse.portudp = port_attribue;
            //clé remplie de zéros (aucune sécurité pour l'instant)
            memset(reponse.cle, 0, 113); 
        }

        // on déverrouille l'accès
        pthread_mutex_unlock(&mutex_annuaire);

        if (reponse.codereq == 2) {
            serialize_inscription_resp(&reponse, buffer_envoi);
            my_send(sockclient, buffer_envoi, RESP_INSCRIPTION_SIZE);
        }
    }

    // on ferme la connexion TCP dès l'échange terminé
    close(sockclient);
    return NULL;
}

int main(void) {
    // Initialisation du serveur TCP IPv6
    int sock_serveur = init_serveur_tcp_ipv6(PORT_SERVEUR);
    if (sock_serveur < 0) return EXIT_FAILURE;

    pthread_t tpthread[MAX_USERS];
    int index_thread = 0;

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
        if (pthread_create(&tpthread[index_thread], NULL, serve, sockclient) == 0) {
            index_thread++;
        } else {
            close(*sockclient);
            free(sockclient);
        }
    }

    close(sock_serveur);
    pthread_mutex_destroy(&mutex_annuaire);
    return 0;
    
}