# Froxy(A)

### Rulare:

Se ruleaza `make -f Makefile` intr-un terminal pentru compilarea surselor.
Pentru partea de server se ruleaza `./server` intr-un terminal separat si se pot observa in acesta toate etapele de primire si executare a diverselor comenzi.
Pentru partea de client se ruleaza `./client` intr-un terminal separat corespunzator fiecarui client nou si se vor urma instructiunile din acesta.

### Clientul:

![Client](/documentatie/Interfata client.png)

### Cerinta:

Sa se implementeze o aplicatie 'proxy' pentru protocolul FTP. Aplicatia va include atat o componenta server (proxy-ul in sine) cat si o componenta client. Serverul (proxy-ul) va juca rol de client FTP pentru un alt server FTP de la care va prelua si stoca temporar fisiere. Dimensiunea maxima si tipurile fisierelor stocate temporar de acest proxy se vor specifica intr-un fisier de configurare. Acest fisier va mai contine si o lista de situri interzise, pe baza unor politici de interzicere a accesului la ele (e.g., interzicerea accesului la siturile FTP din domeniul .uaic.ro intre orele 08-20, in zilele de luni-vineri, pentru clienti provenind din domeniul .y.ro). Componenta client ofera unui utilizator obisnuit posibilitatea de download a fisierelor stocate pe proxy si in plus, unui administrator, posibilitatea de a modifica fisierul de configurare.

Resurse suplimentare: [RFC 959](https://www.ietf.org/rfc/rfc959.txt)
