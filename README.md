# Progetto Multi-Client LSO - Telefono Senza Fili

Un sistema multi-client, composto da:
- **Server** scritto in C
- **Client** scritto in Python

### Installazione Librerie
Per compilare ed eseguire il sistema, prima installare le seguenti librerie:

```bash
sudo apt-get install uuid-dev libglib2.0-dev
```
## Compilazione ed Esecuzione

### Compilare il server

Aprire il terminale nella cartella ./server del progetto ed eseguire:

```bash
gcc server.c -o server -luuid $(pkg-config --cflags --libs glib-2.0)
```

### Esecuzione
```bash
./server
```

### Compilare ed eseguire il client

Aprire il terminale nella cartella ./client del progetto ed eseguire:

```bash
python3 client.py
```

