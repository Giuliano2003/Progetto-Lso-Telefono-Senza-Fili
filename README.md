# Multi-Client Project LSO - Telephone Game

This project implements a multilingual "Telephone Game" as a distributed system. Multiple clients can connect to a central server, create or join lobbies, and play a word game where phrases are passed and translated between players in different languages. The server manages user authentication, lobby creation, player queues, and match logic, including phrase translation using an external translation service.

**Technical details:**
- **Server:** Written in C, uses sockets for TCP communication, manages lobbies and matches, and integrates with [LibreTranslate](https://libretranslate.com/) via HTTP for phrase translation. User data is stored in SQLite.
- **Client:** Python GUI client using Tkinter for an interactive experience. Also includes a C command-line client for terminal play.
- **Translation:** The server communicates with a LibreTranslate container to translate phrases between players' chosen languages.
- **Docker:** The server and translation service are containerized for easy deployment.
- **Features:** User signup/login, lobby management, player queueing, match direction (clockwise/counter-clockwise), phrase translation, and match history display.

# Technical Architecture

- **Multithreading:**  
  The server is fully multithreaded. For each new client connection, a dedicated thread is spawned using POSIX threads (`pthread_create`). This allows the server to handle multiple clients concurrently, ensuring responsiveness and scalability. Shared resources such as the global player and lobby tables are protected using mutexes to prevent race conditions.

- **Socket Communication:**  
  The server uses TCP sockets for reliable communication. It listens on a configurable port (default: 8080) and accepts incoming client connections. Each client communicates with the server using a simple text-based protocol, where each message starts with an operation code followed by any required parameters.

- **Synchronization:**  
  To ensure thread safety, mutexes are used around critical sections, such as modifying the list of players, lobbies, and sending data over sockets. Each player's socket is protected by its own mutex to avoid concurrent writes.

- **Lobby and Match Management:**  
  Lobbies and matches are managed in-memory using GLib data structures (`GHashTable`, `GList`, `GQueue`). Each lobby has its own mutex for managing its player list and queue. The server enforces limits on the number of lobbies and players per lobby.

- **Database:**  
  User credentials and preferences are stored in an SQLite database. The server initializes the database on startup and uses prepared statements for secure access.

- **Translation Service:**  
  The server integrates with LibreTranslate via HTTP requests to translate phrases between languages during the game.

## Server Protocol

The server and clients communicate using a simple text-based protocol over TCP sockets. Each message starts with an operation code (OP) or response code, followed by any required parameters, separated by spaces or newlines.

### Operation Codes (Client → Server)

| Code | Operation           | Format / Parameters                                  |
|------|---------------------|-----------------------------------------------------|
| 201  | Signup              | `201 <lang> <username> <password>`                  |
| 202  | Login               | `202 <username> <password>`                         |
| 100  | Create Lobby        | `100`                                               |
| 101  | Join Lobby          | `101 <lobby_id>`                                    |
| 102  | Get Lobbies         | `102`                                               |
| 103  | Leave Lobby         | `103`                                               |
| 110  | Start Match         | `110 <direction>` (`1` for clockwise, `0` for counter) |
| 111  | Speak (add word)    | `111 <len> <word>`                                  |

### Response Codes (Server → Client)

| Code | Meaning                      | Description                                   |
|------|------------------------------|-----------------------------------------------|
| A00  | Lobby Created                | Lobby successfully created                    |
| A01  | Lobby Joined                 | Successfully joined a lobby                   |
| A02  | Host Left                    | Host left, lobby closed                       |
| A03  | Player Left                  | Player left the lobby                         |
| A04  | Player Enqueued              | Lobby full, player added to queue             |
| A05  | Lobbies List                 | List of available lobbies                     |
| A06  | Queue Left                   | Player left the queue                         |
| A07  | Queue Joined                 | Match started, player added to queue          |
| A08  | Player Joined                | A player joined the lobby                     |
| A10  | Match Started                | Match has started                             |
| A11  | Your Turn                    | It's your turn                                |
| A12  | Match Terminated             | Match ended, phrase history shown             |
| A13  | Wait for Others              | Wait for other players                        |
| B01  | Signed Up                    | Signup successful                             |
| B02  | Logged In                    | Login successful                              |
| Z00  | Server Error                 | Internal server error                         |
| Z01  | Bad Request                  | Invalid request format or parameters          |
| Z02  | Conflict                     | Username exists, already logged in, etc.      |
| Z03  | Unauthorized                 | Not authenticated or wrong credentials        |

**Note:**  
- Each message may include additional information after the code, separated by newlines.
- The protocol is designed to be simple and human-readable for debugging and extensibility.

## Build and Run the server
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