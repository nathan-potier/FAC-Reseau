#ifndef UTILS_CLIENT_H
#define UTILS_CLIENT_H

// Fonction qui va tourner en boucle en arrière-plan pour écouter le serveur
void *ecoute_notifications_udp(void *arg);

// Retourne 1 si le serveur a renvoyé une erreur, 0 sinon.
int check_erreur_serveur(const uint8_t *buffer, int recu);

int initialiser_profil(uint16_t *mon_id, uint16_t *mon_port_udp, char *cle_serveur);
void cmd_create(char *nom_groupe, uint16_t mon_id);
void cmd_invite(char *args, uint16_t mon_id);
void cmd_li(uint16_t mon_id);
void cmd_join(char *id_groupe_str, uint16_t mon_id);
void cmd_decline(char *id_groupe_str, uint16_t mon_id);
void cmd_members(char *id_groupe_str, uint16_t mon_id);
void cmd_post(char *args, uint16_t mon_id);

#endif