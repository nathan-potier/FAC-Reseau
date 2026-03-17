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

