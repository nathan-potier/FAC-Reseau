#define UTILS_H

#include <stddef.h>
#include <sys/types.h>

// Initialise un serveur TCP IPv6 en écoute sur le port donné
int init_serveur_tcp_ipv6(int port);

// Connecte un client TCP IPv6 à une adresse et un port donnés
int init_client_tcp_ipv6(const char *adresse, int port);

// Envoie tout les octets demandés 
ssize_t my_send(int sock, const void *buf, size_t len);

// Reçoit toute la quantité demandée 
ssize_t my_recv(int sock, void *buf, size_t len);

// Initialise une socket UDP IPv6 en écoute sur un port donné
int init_ecoute_udp_ipv6(int port);

// Abonne une socket UDP existante à un groupe Multicast IPv6
int rejoindre_multicast_ipv6(int sock, const char *adresse_ip_multicast);

// Génère une adresse Multicast IPv6 unique basée sur l'id du groupe
void generer_adresse_multicast(uint16_t id_groupe, char *ip_str_sortie, uint8_t *ip_bin_sortie);

// envoie un messahe en UDP vers une adresse IPv6 (Unicast ou Multicast)
int envoyer_message_udp_ipv6(const char *ip_dest, int port_dest, const uint8_t *message, size_t len);

// Envoie une erreur TCP (CODEREQ = 31) avec un message
void envoyer_erreur_tcp(int sock, const char *message_erreur);