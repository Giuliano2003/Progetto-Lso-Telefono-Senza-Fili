# Progetto Multi-Client LSO - Telefono Senza Fili

Un sistema multi-client, composto da:
- **Server** scritto in C
- **Client** scritto in Python

### Installazione Librerie
Per compilare ed eseguire il sistema, prima installare le seguenti librerie:

```bash
sudo apt-get install uuid-dev
```
## Compilazione ed Esecuzione

### Compilare il server

Aprire il terminale nella cartella client-server del progetto ed eseguire:

```bash
gcc server.c -o server -luuid
```

### Esecuzione
```bash
./server
```

### Compilare ed eseguire il client

Aprire il terminale nella cartella client-server del progetto ed eseguire:

```bash
python3 client_guy.py
```

