# Protocoale de comunicatii
# Laborator 7 - TCP
# Echo Server
# Makefile

CFLAGS = -Wall -g -Werror -Wno-error=unused-variable 

# Portul pe care asculta serverul
PORT = 12345

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

all: server subscriber

common.o: common.c

# Compileaza server.c
server: server.c common.o
	$(CC) $(CFLAGS) -o server server.c common.o -lm

# Compileaza client.c
subscriber: subscriber.c common.o
	$(CC) $(CFLAGS) -o subscriber subscriber.c common.o -lm

.PHONY: clean run_server run_client

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza clientul 	
run_client:
	./client ${IP_SERVER} ${PORT}

clean:
	rm -rf server subscriber *.o *.dSYM
