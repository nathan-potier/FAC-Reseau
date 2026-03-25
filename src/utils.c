#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "utils.h"




int init_serveur_tcp_ipv6(int port) {
    int sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket serveur");
        return -1;
    }

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));


    struct sockaddr_in6 adrsock;
    memset(&adrsock, 0, sizeof(adrsock));
    adrsock.sin6_family = AF_INET6;
    adrsock.sin6_port = htons(port);
    adrsock.sin6_addr = in6addr_any;

    if (bind(sock, (struct sockaddr *) &adrsock, sizeof(adrsock)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    if (listen(sock, 0) < 0) { 
        perror("listen");
        close(sock);
        return -1;
    }

    return sock;
}


int init_client_tcp_ipv6(const char *adresse, int port){
    int sock = socket(PF_INET6, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket client");
        return -1;
    }

    struct sockaddr_in6 adrserv;
    memset(&adrserv, 0, sizeof(adrserv));
    adrserv.sin6_family = AF_INET6;
    adrserv.sin6_port = htons(port);


    if (inet_pton(AF_INET6, adresse, &adrserv.sin6_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&adrserv, sizeof(adrserv)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}



ssize_t my_send(int sock, const void *buf, size_t len) {
    size_t total_envoye = 0;
    // cast en char* pour manipuler octet par octet
    const char *pbuf = (const char *)buf; 

    while (total_envoye < len) {
        ssize_t envoye = send(sock, pbuf + total_envoye, len - total_envoye, 0);
        if (envoye <= 0) {
            return envoye; // erreur ou déconnexion
        }
        total_envoye += envoye;
    }
    return total_envoye;
}

ssize_t my_recv(int sock, void *buf, size_t len) {
    size_t total_recu = 0;
    char *pbuf = (char *)buf;

    while (total_recu < len) {
        ssize_t recu = recv(sock, pbuf + total_recu, len - total_recu, 0);
        if (recu <= 0) {
            return recu; // erreur ou déconnexion
        }
        total_recu += recu;
    }
    return total_recu;
}



int init_ecoute_udp_ipv6(int port) {
    int sock = socket(PF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Erreur création socket UDP");
        return -1;
    }
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in6 adr_ecoute;
    memset(&adr_ecoute, 0, sizeof(adr_ecoute));
    adr_ecoute.sin6_family = AF_INET6;
    adr_ecoute.sin6_port = htons(port);
    adr_ecoute.sin6_addr = in6addr_any;

    if (bind(sock, (struct sockaddr *)&adr_ecoute, sizeof(adr_ecoute)) < 0) {
        perror("Erreur bind UDP");
        close(sock);
        return -1;
    }

    return sock;
}


int rejoindre_multicast_ipv6(int sock, const char *adresse_ip_multicast) {
    struct ipv6_mreq group;
    if (inet_pton(AF_INET6, adresse_ip_multicast, &group.ipv6mr_multiaddr) <= 0) {
        perror("Erreur inet_pton multicast");
        return -1;
    }

    // 0 = interface par défaut
    group.ipv6mr_interface = 0;

    if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &group, sizeof(group)) < 0) {
        perror("Erreur abonnement groupe multicast");
        return -1;
    }

    return 0;
}

void generer_adresse_multicast(uint16_t id_groupe, char *ip_str_sortie, uint8_t *ip_bin_sortie){

    sprintf(ip_str_sortie, "ff12::%x", id_groupe);

    inet_pton(AF_INET6, ip_str_sortie, ip_bin_sortie);
}