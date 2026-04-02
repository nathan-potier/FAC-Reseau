#ifndef UTILS_CLIENT_H
#define UTILS_CLIENT_H

// Fonction qui va tourner en boucle en arrière-plan pour écouter le serveur
void *ecoute_notifications_udp(void *arg);

// Retourne 1 si le serveur a renvoyé une erreur, 0 sinon.
int check_erreur_serveur(const uint8_t *buffer, int recu);

#endif