#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define BUFFER_SIZE 4096
#define MAX_INPUT 256

// Protocol operation codes
#define OP_CREATE_LOBBY 100
#define OP_JOIN_LOBBY 101
#define OP_GET_LOBBIES 102
#define OP_LEAVE_LOBBY 103
#define OP_START_MATCH 110
#define OP_SPEAK 111
#define OP_SIGNUP 201
#define OP_LOGIN 202

typedef struct {
    int socket;
    bool connected;
    bool authenticated;
    bool in_lobby;
    bool is_host;
    char username[32];
    pthread_mutex_t output_mutex;
} ClientState;

ClientState client_state = {0};

void print_help() {
    printf("\n=== Word Game Client Commands ===\n");
    printf("Authentication:\n");
    printf("  signup <lang> <username> <password> - Register new account\n");
    printf("  login <username> <password>         - Login to account\n");
    printf("\nLobby Management:\n");
    printf("  create                              - Create new lobby (host only)\n");
    printf("  join <lobby_id>                     - Join existing lobby\n");
    printf("  lobbies                             - List available lobbies\n");
    printf("  leave                               - Leave current lobby\n");
    printf("\nGame Commands:\n");
    printf("  start <0|1>                         - Start match (0=counter-clockwise, 1=clockwise)\n");
    printf("  say <word/phrase>                   - Speak during your turn\n");
    printf("\nOther:\n");
    printf("  help                                - Show this help\n");
    printf("  quit                                - Exit client\n");
    printf("=====================================\n\n");
}

void print_status() {
    pthread_mutex_lock(&client_state.output_mutex);
    printf("\n--- Status ---\n");
    printf("Connected: %s\n", client_state.connected ? "Yes" : "No");
    printf("Authenticated: %s\n", client_state.authenticated ? "Yes" : "No");
    if (client_state.authenticated) {
        printf("Username: %s\n", client_state.username);
    }
    printf("In Lobby: %s\n", client_state.in_lobby ? "Yes" : "No");
    if (client_state.in_lobby) {
        printf("Host: %s\n", client_state.is_host ? "Yes" : "No");
    }
    printf("-------------\n\n");
    pthread_mutex_unlock(&client_state.output_mutex);
}

void parse_server_response(const char* response) {
    pthread_mutex_lock(&client_state.output_mutex);
    
    char code[4];
    strncpy(code, response, 3);
    code[3] = '\0';
    
    printf("Server: ");
    
    // Parse response codes
    if (strcmp(code, "A00") == 0) {
        printf("✓ Lobby created!\n");
        client_state.in_lobby = true;
        client_state.is_host = true;
        // Print lobby ID if present (after newline)
        const char* lobby_id = strchr(response, '\n');
        if (lobby_id && *(lobby_id + 1)) {
            lobby_id++; // move past '\n'
            printf("Your lobby ID is: %s\n", lobby_id);
            printf("Share this ID with other players so they can join.\n");
        }
    }
    else if (strcmp(code, "A01") == 0) {
        printf("✓ Joined lobby!\n");
        client_state.in_lobby = true;
    }
    else if (strcmp(code, "A02") == 0) {
        printf("⚠ Host left the lobby\n");
        client_state.in_lobby = false;
        client_state.is_host = false;
    }
    else if (strcmp(code, "A03") == 0) {
        printf("ℹ A player left the lobby\n");
    }
    else if (strcmp(code, "A04") == 0) {
        printf("ℹ Added to queue (lobby full)\n");
        client_state.in_lobby = true;
    }
    else if (strcmp(code, "A05") == 0) {
        printf("ℹ No active lobbies\n");
    }
    else if (strcmp(code, "A06") == 0) {
        printf("✓ Left the queue\n");
        client_state.in_lobby = false;
    }
    else if (strcmp(code, "A07") == 0) {
        printf("ℹ You are in queue\n");
        client_state.in_lobby = true;
    }
    else if (strcmp(code, "A10") == 0) {
        printf("🎮 Match started!\n");
    }
    else if (strcmp(code, "A11") == 0) {
        printf("🎯 It's your turn!\n");
    }
    else if (strcmp(code, "A12") == 0) {
        printf("🏁 Match ended/terminated\n");
    }
    else if (strcmp(code, "A13") == 0) {
        printf("⏳ Wait for the other players\n");
    }
    else if (strcmp(code, "B01") == 0) {
        printf("✓ Signup successful!\n");
    }
    else if (strcmp(code, "B02") == 0) {
        printf("✓ Login successful!\n");
        client_state.authenticated = true;
    }
    else if (strcmp(code, "Z00") == 0) {
        printf("❌ Server error\n");
    }
    else if (strcmp(code, "Z01") == 0) {
        printf("❌ Bad request\n");
    }
    else if (strcmp(code, "Z02") == 0) {
        printf("❌ Conflict\n");
    }
    else if (strcmp(code, "Z03") == 0) {
        printf("❌ Unauthorized\n");
    }
    else {
        // Handle lobby list or other responses
        printf("Response:\n");
    }
    
    // Print the rest of the response (after the code)
    if (strlen(response) > 3) {
        printf("%s\n", response + 4);
    }

    // Print prompt on a new line after server response
    printf("> ");
    fflush(stdout);

    pthread_mutex_unlock(&client_state.output_mutex);
}

void* receive_thread(void* arg) {
    char buffer[BUFFER_SIZE];
    
    while (client_state.connected) {
        int bytes = recv(client_state.socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            pthread_mutex_lock(&client_state.output_mutex);
            printf("Connection lost.\n");
            pthread_mutex_unlock(&client_state.output_mutex);
            client_state.connected = false;
            break;
        }
        
        buffer[bytes] = '\0';
        parse_server_response(buffer);
    }
    
    return NULL;
}

bool connect_to_server(const char* host, int port) {
    client_state.socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_state.socket < 0) {
        perror("Socket creation failed");
        return false;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_state.socket);
        return false;
    }
    
    if (connect(client_state.socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_state.socket);
        return false;
    }
    
    client_state.connected = true;
    return true;
}

void send_command(const char* command) {
    if (!client_state.connected) {
        printf("Not connected to server.\n");
        return;
    }
    
    send(client_state.socket, command, strlen(command), 0);
}

void handle_command(const char* input) {
    char command[MAX_INPUT];
    char arg1[MAX_INPUT] = {0};
    char arg2[MAX_INPUT] = {0};
    char arg3[MAX_INPUT] = {0};
    
    int args = sscanf(input, "%s %s %s %s", command, arg1, arg2, arg3);
    
    if (strcmp(command, "help") == 0) {
        print_help();
    }
    else if (strcmp(command, "status") == 0) {
        print_status();
    }
    else if (strcmp(command, "signup") == 0) {
        if (args != 4) {
            printf("Usage: signup <lang> <username> <password>\n");
            printf("Example: signup en myusername mypassword\n");
            return;
        }
        char msg[MAX_INPUT];
        snprintf(msg, sizeof(msg), "201 %s %s %s", arg1, arg2, arg3);
        send_command(msg);
    }
    else if (strcmp(command, "login") == 0) {
        if (args != 3) {
            printf("Usage: login <username> <password>\n");
            return;
        }
        char msg[MAX_INPUT];
        snprintf(msg, sizeof(msg), "202 %s %s", arg1, arg2);
        strncpy(client_state.username, arg1, sizeof(client_state.username) - 1);
        send_command(msg);
    }
    else if (strcmp(command, "create") == 0) {
        if (!client_state.authenticated) {
            printf("You must login first.\n");
            return;
        }
        send_command("100");
    }
    else if (strcmp(command, "join") == 0) {
        if (!client_state.authenticated) {
            printf("You must login first.\n");
            return;
        }
        if (args != 2) {
            printf("Usage: join <lobby_id>\n");
            return;
        }
        char msg[MAX_INPUT];
        snprintf(msg, sizeof(msg), "101 %s", arg1);
        send_command(msg);
    }
    else if (strcmp(command, "lobbies") == 0) {
        if (!client_state.authenticated) {
            printf("You must login first.\n");
            return;
        }
        send_command("102");
    }
    else if (strcmp(command, "leave") == 0) {
        if (!client_state.authenticated) {
            printf("You must login first.\n");
            return;
        }
        if (!client_state.in_lobby) {
            printf("You are not in a lobby.\n");
            return;
        }
        send_command("103");
        client_state.in_lobby = false;
        client_state.is_host = false;
    }
    else if (strcmp(command, "start") == 0) {
        if (!client_state.authenticated) {
            printf("You must login first.\n");
            return;
        }
        if (!client_state.is_host) {
            printf("Only the host can start a match.\n");
            return;
        }
        if (args != 2 || (arg1[0] != '0' && arg1[0] != '1')) {
            printf("Usage: start <0|1> (0=counter-clockwise, 1=clockwise)\n");
            return;
        }
        char msg[MAX_INPUT];
        snprintf(msg, sizeof(msg), "110 %c", arg1[0]);
        send_command(msg);
    }
    else if (strcmp(command, "say") == 0) {
        if (!client_state.authenticated) {
            printf("You must login first.\n");
            return;
        }
        if (!client_state.in_lobby) {
            printf("You must be in a lobby to speak.\n");
            return;
        }
        
        // Find the start of the phrase (after "say ")
        const char* phrase_start = strchr(input, ' ');
        if (!phrase_start || strlen(phrase_start + 1) == 0) {
            printf("Usage: say <word or phrase>\n");
            return;
        }
        phrase_start++; // Skip the space
        
        int phrase_len = strlen(phrase_start);
        if (phrase_len > 30) {
            printf("Phrase too long (max 30 characters).\n");
            return;
        }
        
        char msg[MAX_INPUT];
        snprintf(msg, sizeof(msg), "111 %02d %s", phrase_len, phrase_start);
        send_command(msg);
    }
    else if (strcmp(command, "quit") == 0) {
        printf("Goodbye!\n");
        client_state.connected = false;
    }
    else {
        printf("Unknown command: %s\n", command);
        printf("Type 'help' for available commands.\n");
    }
}

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    int port = 8080;
    
    // Parse command line arguments
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    
    pthread_mutex_init(&client_state.output_mutex, NULL);
    
    printf("=== Word Game Client ===\n");
    printf("Connecting to %s:%d...\n", host, port);
    
    if (!connect_to_server(host, port)) {
        printf("Failed to connect to server.\n");
        return 1;
    }
    
    printf("Connected! Type 'help' for commands.\n\n");
    
    // Start receive thread
    pthread_t receive_tid;
    pthread_create(&receive_tid, NULL, receive_thread, NULL);
    
    // Main input loop
    char input[MAX_INPUT];
    while (client_state.connected) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) {
            continue;
        }
        
        handle_command(input);
    }
    
    // Cleanup
    if (client_state.connected) {
        close(client_state.socket);
    }
    
    pthread_join(receive_tid, NULL);
    pthread_mutex_destroy(&client_state.output_mutex);
    
    return 0;
}