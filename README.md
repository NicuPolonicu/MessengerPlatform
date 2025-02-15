// Nicula Dan-Alexandru, 322CA

Tema 2, Aplicatie server-subscriber peste TCP

Continut tema:

    -server.c: contine implementarea server-ului, care accepta conexiuni TCP (subscriberi)
si UDP (mesaje pentru subscriberi)
    -subscriber.c: contine implementarea clientului TCP, care se conecteaza la server, trimite
mesaje de subscribe/unsubscribe/exit server-ului, si primeste de la server mesajele topic-urilor
la care este abonat
    -common.c: implementari pentru recv_all si send_all, utilizate extensiv in server si subscriber
    -utils.h: macro-ul DIE
    -Makefile: pentru compilarea codului sursa

Functionalitate:

    Intai trebuie compilate fisierele cu cod, folosind comanda:
        make

    Server-ul porneste cu un singur parametru de la CLI:
        ./server <PORT>

    Odata pornit acesta accepta atat conexiuni TCP, cat si UDP, astepandu-le folosing poll.
    Server-ul accepta si intrare de la tastatura, singura comanda fiind exit pentru inchiderea
conexiunilor (implicit si a clientilor) si server-ului.
    Subscriber-ul (clientul TCP) se ruleaza cu urmatoarea comanda:
        /subscriber <SUBSCRIBER_ID> <SERVER_IP> <SERVER_PORT>

    Odata stabilita o conexiune intre un subscriber si server, comunicarea se face prin
pachete TCP. Mesajele de la client la server contin tipul comenzii (SUBSCRIBE, UNSUBSCRIBE
sau EXIT, reprezentat printr-un uint8_t) si topicul pentru care are loc comanda. (mesajul
se scrie intr-un struct TCPMessage)
    Daca server-ul primeste o datagrama prin UDP (de la un client UDP care transmite mesajele
pentru topic-uri), se vor procesa datele din aceasta intr-un nou pachet TCP (struct TCPSubMessage)
care va fi trimis la cei abonati la topic-ul din datagrama.
    ATENTIE! Nu se va trimite intreaga lungime a structurii de date la fiecare trimitere catre
clienti, intrucat unele mesaje sunt mai scurte decat altele. De aceea, atunci cand server-ul trimite
date la client, va trimite mai intai un uint16_t ce reprezinta lungimea informatiei ce urmeaza sa fie
trimisa, apoi mesajul respectiv. Astfel, pentru un mesaj de tip INT se vor trimite in jur de 80 de octeti,
nu ~1600 cat are un struct intreg. (datele trimise prin TCP se vor scrie in structura TCPSubMessage)
    P.S.: Am incercat sa fac alocare dinamica pentru vectorul de topicuri ale unui client (subscribed_topics),
insa in checker mergea mult prea incet pentru a fi considerat corect. (mesajele se afisau foarte lent) De aceea 
am renuntat la asta, insa clientii sunt alocati dinamic :)


