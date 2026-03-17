all: serveur client

serveur: serveur.c
	gcc -Wall -pthread serveur.c -o serveur

client: client.c
	gcc -Wall -pthread client.c -o client

clean:
	rm -f serveur client