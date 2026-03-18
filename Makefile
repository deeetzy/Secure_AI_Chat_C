# Variabile pentru compilator și flag-uri
CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lssl -lcrypto -lpthread -lcurl

# Numele executabilelor
SERVER = server
CLIENT = client

# Sursă fișiere
SERVER_SRC = server.c
CLIENT_SRC = client.c

# Regula default (compilează tot)
all: $(SERVER) $(CLIENT)

# Compilare Server
$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER) $(LIBS)

# Compilare Client
$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o $(CLIENT) -lssl -lcrypto

# Generare certificate SSL (dacă nu le ai deja)
certs:
	openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout server.key -out server.crt

# Curățare fișiere binare
clean:
	rm -f $(SERVER) $(CLIENT)

# Rulare Server
run-server:
	./$(SERVER)

# Rulare Client
run-client:
	./$(CLIENT)

.PHONY: all clean certs run-server run-client
