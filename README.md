# Multi-Client Project LSO - Telephone Game

This project implements a multilingual "Telephone Game" as a distributed system. Multiple clients can connect to a central server, create or join lobbies, and play a word game where phrases are passed and translated between players in different languages. The server manages user authentication, lobby creation, player queues, and match logic, including phrase translation using an external translation service.

**Technical details:**
- **Server:** Written in C, uses sockets for TCP communication, manages lobbies and matches, and integrates with [LibreTranslate](https://libretranslate.com/) via HTTP for phrase translation. User data is stored in SQLite.
- **Client:** Python GUI client using Tkinter for an interactive experience. Also includes a C command-line client for terminal play.
- **Translation:** The server communicates with a LibreTranslate container to translate phrases between players' chosen languages.
- **Docker:** The server and translation service are containerized for easy deployment.
- **Features:** User signup/login, lobby management, player queueing, match direction (clockwise/counter-clockwise), phrase translation, and match history display.

## Server
### Prerequisites
Make sure Docker is installed on your machine.
Navigate to the `server` directory.

### Build
```bash
docker-compose build server
```

### Run
```bash
docker-compose up
```

## Client
Open a terminal in the project's `client` folder and run:

```bash
python3 client.py
```