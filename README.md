# Server-Subscriber Application over TCP

**Autor:** Nicula Dan-Alexandru - 322CA

## Descriere
Această aplicație implementează un sistem de comunicare server-subscriber bazat pe TCP și UDP. Server-ul gestionează conexiunile și mesajele între subscriberi și sursele de date.

---

## Structura Proiectului

- **`server.c`** - Implementarea serverului, care acceptă conexiuni TCP (subscriberi) și UDP (mesaje pentru subscriberi).
- **`subscriber.c`** - Implementarea clientului TCP, care se conectează la server, trimite mesaje de subscribe/unsubscribe/exit și primește mesaje pentru topic-urile abonate.
- **`common.c`** - Funcții auxiliare pentru `recv_all` și `send_all`, utilizate extensiv în server și subscriber.
- **`utils.h`** - Macro-ul `DIE` pentru gestionarea erorilor.
- **`Makefile`** - Script pentru compilarea codului sursă.

---

## Instrucțiuni de Utilizare

### 1. Compilare

Pentru a compila fișierele sursă, rulați comanda:

```sh
make
```

### 2. Pornirea Serverului

Serverul trebuie pornit cu un singur parametru, portul pe care ascultă:

```sh
./server <PORT>
```

Acesta acceptă conexiuni **TCP** și **UDP**, utilizând `poll` pentru gestionarea evenimentelor. Serverul poate primi și comenzi de la tastatură, singura comandă fiind:

```sh
exit
```

Aceasta închide toate conexiunile (inclusiv clienții activi) și oprește serverul.

### 3. Pornirea unui Subscriber

Subscriber-ul (clientul TCP) se pornește folosind:

```sh
./subscriber <SUBSCRIBER_ID> <SERVER_IP> <SERVER_PORT>
```

După stabilirea conexiunii, subscriber-ul comunică cu serverul prin pachete **TCP**.

---

## Funcționalitate

### 1. Comunicarea între Subscriber și Server

Mesajele trimise de client către server conțin:
- Tipul comenzii (`SUBSCRIBE`, `UNSUBSCRIBE` sau `EXIT` - reprezentat prin `uint8_t`)
- Numele topic-ului aferent comenzii

Aceste mesaje sunt structurate în `TCPMessage`.

### 2. Recepționarea Mesajelor UDP de către Server

- Serverul primește datagrame **UDP** care conțin mesaje pentru diverse topic-uri.
- Mesajele sunt procesate într-un pachet **TCP** (`TCPSubMessage`), apoi trimise tuturor subscriberilor înregistrați pentru acel topic.

### 3. Optimizarea Trimiterii Mesajelor TCP

Pentru a minimiza traficul și a optimiza performanța:
- Serverul trimite mai întâi un `uint16_t` ce specifică **dimensiunea** informației ce urmează să fie transmisă.
- Apoi trimite mesajul propriu-zis.

Exemplu:
- Un mesaj de tip **INT** are aproximativ **80 de octeți**.
- Nu se trimit **~1600 octeți** (dimensiunea maximă a structurii `TCPSubMessage`).

---

## Observații

- Inițial, am încercat să folosesc **alocare dinamică** pentru vectorul `subscribed_topics` al fiecărui client.
- Totuși, checker-ul rula extrem de lent în acest caz.
- Din acest motiv, am renunțat la alocarea dinamică pentru topic-uri, însă **clienții sunt alocați dinamic**.

---