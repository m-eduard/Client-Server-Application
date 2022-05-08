Marin Eduard-Constantin, 321CA


*Protocolul de transmitere a datelor peste TCP:

|-- Scop: delimitarea si receptionarea mesajelor integral, atunci cand
|         prin TCP apar concatenari sau trunchieri ale segmentelor trimise
|
|-- Format: header + mesaj
|           |
|           |-- byte de control, folosit pentru a delimita mesajele goale de
|           |   celelalte mesaje trimise prin TCP:
|           |   |-- astfel, din stream-ul pastrat in buffer-ul pentru TCP se 
|           |   |   extrage mai intai un byte; daca aceesta nu exista, functia
|           |   |   recv() va intoarce 0, iar folosind faptul ca s-a primit un
|           |   |   string empty, se stie ca un anumit client s-a deconectat
|           |   |   de la server
|           |   |
|           |   |-- altfel, byte-ul primit va trebui sa fie identic cu valoarea
|           |   |   asociata byte-ul de control (memorata intr-un header), iar
|           |   |   in continuare se va parcurge restul header-ului
|           |   |______________________________________________________________
|           |
|           |-- dupa byte-ul de control, urmatorii 4 bytes (uint32_t) desemnea-
|           |   za lungimea mesajului care trebuie citit
|           |__________________________________________________________________
|
| -- Stiind lungimea mesajului, intr-o bucla se tot primesc segmente din
|    stream-ul TCP, pana cand au fost cititi un numar de bytes egali cu
|    dimensiunea mesajului
|______________________________________________________________________________


**Protocolul de trimitere a mesajelor UDP catre clientii TCP:

|-- in cadrul unei structuri, sunt memorate adresa IP si portul de
|   pe care a venit mesajul UDP, precum si payload-ul acestuia, fiind
|   folosit un union pentru a delimita cele 4 tipuri de date din mesaje
|
|--| transmiterea mesajelor catre clientii TCP va fi diferita, in
|  | functie de tipul mesajului:
|  |
|  |---- non-STRING:
|  |   - serverul va trimite catre clientii TCP direct structura
|  |     in care au fost parsate datele utile din mesajul primit pe UDP
|  |
|  |---- STRING:
|  |   - se va trimite un mesaj continand structura, la fel ca in
|  |     primul caz, la care va fi concatenat string-ul care reprezinta
|  |     partea de date din mesajul primit pe UDP, din cauza faptului ca
|  |     in structura este memorat doar un pointer catre string,
|  |     a carui semnificatie se va pierde cand va ajunge in client
|  |___________________________________________________________________________
|
|-- serverul va trimite doar datele utile din payload-ul UDP, fara a aplica
|   vreo procedura de decodare a acestora (de exemplu recrearea unui FLOAT,
|   folosind byte-ul de semn si partea intreaga/fractionara), deci in clientii
|   TCP urmeaza sa fie creat string-ul care va fi afisat la stdout, decodat si
|   formatat conform regulilor care descriu mesajele trimise de clientii UDP
|   (am decis ca doar clientii TCP sa decodeze datele, deoarece decodarea si
|    transformarea acestora in string ar putea fi o operatie costisitoare
|    pentru server, care ar deveni supraincarcat in cazul unui numar foarte
|    mare de mesaje)
|______________________________________________________________________________

____________________________________________________________________________
!! Toate mesajele care vor fi trimise mai departe prin TCP vor folosi
protocolul *, fiind folosite functiile de receive_message() / send_message()
in locul recv() / send().
____________________________________________________________________________


Subscriber:

Conectare:

Subscriber-ul trimite o cerere de conexiune catre server, urmand sa
primeasca un raspuns in functie de disponibilitatea ID-ului folosit.
Astfel, daca in server exista deja un alt subscriber activ cu acelasi
ID, subscriberul care incearca sa se conecteze primeste un mesaj de
REJECT, fiind refuzata conexiunea. Altfel, este stabilita o conexiune
intre server si noul subscriber.

Afisare mesaje forwardate de server de la clientii UDP:

In functie de tipul payload-ului continut de mesaj, datele sunt
interpretate si este construit un string care va fi afisat la stdin,
reprezentand informatia codificata din mesaj.
____________________________________________________________________

Server:

Pentru a putea primi simultan date de pe mai multi file descriptori,
am folosit multiplexarea I/O intre socket-ul UDP, socket-ul pasiv TCP,
socket-ii activi asignati clientilor TCP si stdin.

Flow:

- Cand serverul primeste date de la clientii UDP, le memoreaza in
  structuri care contin IP-ul si portul clientului UDP care a trimis
  un mesaj, precum si payload-ul continut in mesaj.

  Apoi, trece prin lista clientilor TCP care sunt abonati la topicul
  mesajului, iar daca sunt conectati in acel moment, serverul le
  trimite mesajul primit de pe UDP, incapsulat conform "protocolului
  de trimitere a mesajelor UDP catre clientii TCP" (**).

  Pentru clientii TCP care s-au conectat la un moment dat la server,
  dar nu este stabilita o conexiune in momentul actual, se verifica daca
  este activa optiunea de Store & Forward pentru topicul mesajul curent,
  iar in cazul in care este, serverul pastreaza mesajul intr-o coada
  asociata clientului TCP respectiv, urmand ca la reconectare, toate
  mesajele din acea coada sa ii fie trimise.

- Cand se primeste o cerere de conectare de la un client TCP, daca ID-ul
  clientului nu este conectat deja, serverul adauga noul ID intr-un
  hashmap, asociindu-i acestuia o structura de tip client_t. In plus,
  intr-un alt hashmap, file descriptor-ului asignat acestui nou client ii
  este mapat ID-ul clientului, pentru a putea recunoaste mai tarziu un
  subscriber dupa fd.

- Cand se primeste o comanda de subscribe sau unsubscribe, este adaugat sau
  eliminat noul subscriber intr-un hashmap, astfel incat la cheia cu numele
  topicului la care s-a abonat, este modificat vectorul care retine ID-urile
  abonatilor si valoarea parametrului de Store & Forward.
____________________________________________________________________________
