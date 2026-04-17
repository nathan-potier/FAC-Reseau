CFLAGS = -Wall -Wextra -g -pthread -I./include -lpthread 


all: serveur client

serveur: src/serveur.c src/utils.c
	gcc $(CFLAGS) -o serveur src/serveur.c src/utils.c src/serialisation_serveur.c -lssl -lcrypto

client: src/client.c src/utils.c
	gcc $(CFLAGS) -o client src/client.c src/utils.c src/serialisation_client.c src/utils_client.c -lssl -lcrypto

clean:
	rm -f serveur client src/*.o