#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <sys/types.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

// Initialise un serveur TCP IPv6 en écoute sur le port donné
int init_serveur_tcp_ipv6(int port);

// Connecte un client TCP IPv6 à une adresse et un port donnés
int init_client_tcp_ipv6(const char *adresse, int port);

// Envoie tout les octets demandés 
ssize_t my_send_ssl(SSL *ssl, const void *buf, size_t len);

// Reçoit toute la quantité demandée 
ssize_t my_recv_ssl(SSL *ssl, void *buf, size_t len);

// Initialise une socket UDP IPv6 en écoute sur un port donné
int init_ecoute_udp_ipv6(int port);

// Abonne une socket UDP existante à un groupe Multicast IPv6
int rejoindre_multicast_ipv6(int sock, const char *adresse_ip_multicast);

// Génère une adresse Multicast IPv6 unique basée sur l'id du groupe
void generer_adresse_multicast(uint16_t id_groupe, char *ip_str_sortie, uint8_t *ip_bin_sortie);

// envoie un messahe en UDP vers une adresse IPv6 (Unicast ou Multicast)
int envoyer_message_udp_ipv6(const char *ip_dest, int port_dest, const uint8_t *message, size_t len);

// Envoie une erreur TCP (CODEREQ = 31) avec un message
void envoyer_erreur_tcp_ssl(SSL *ssl, const char *message);

// Génère les fichiers priv-ed.pem et pub-ed.pem 
void generer_cles_ed25519(char *cle_publique_113_octets);

// Signe un message 
int signer_message_ed25519(const uint8_t *message, size_t message_len, uint8_t *signature);

// Vérifie une signature 
int verifier_signature_ed25519(const char *cle_publique_113_octets, const uint8_t *message, size_t message_len, const uint8_t *signature);

#endif