CFLAGS = -Wall -Wextra -g -pthread -I./include -lpthread 


all: serveur client

serveur: src/serveur.c src/utils.c
	gcc $(CFLAGS) -o serveur src/serveur.c src/utils.c src/serialisation_serveur.c -lssl -lcrypto

client: src/client.c src/utils.c
	gcc $(CFLAGS) -o client src/client.c src/utils.c src/serialisation_client.c src/utils_client.c -lssl -lcrypto
keys:
	openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout serv.key -out serv.crt -subj "/CN=localhost"
clean:
	rm -f serveur client src/*.o mon_profil.txt