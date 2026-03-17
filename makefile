CFLAGS = -Wall -Wextra -g -pthread -I./include

all: serveur client

serveur: src/serveur.c src/utils.c
	gcc $(CFLAGS) -o serveur src/serveur.c src/utils.c

client: src/client.c src/utils.c
	gcc $(CFLAGS) -o client src/client.c src/utils.c

clean:
	rm -f serveur client src/*.o