#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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