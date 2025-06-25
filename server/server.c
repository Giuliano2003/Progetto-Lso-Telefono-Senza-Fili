#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <glib-2.0/glib.h>

#define PORT 8080
#define MAX_PLAYERS 10
#define MAX_LOBBIES 5

#define OP_CREATE_LOBBY 100
#define OP_JOIN_LOBBY 101
#define OP_GET_LOBBIES 102
#define OP_LEAVE_LOBBY 103

typedef struct Lobby Lobby;

typedef struct
{
    char id[37];
    char username[32];
    int socket;
    Lobby* lobby;
    bool isHost;
    pthread_mutex_t socket_mutex;
} Player;

struct Lobby
{
    char id[37];
    Player *host;
    int max_players;
    GSList* players;
    pthread_mutex_t players_mutex;
};

typedef struct {
    char id[37];
    char host_username[32];
    int max_players;
    int player_count;
} LobbyDto;


typedef struct {
    char* buffer;
    int idx; //chars written
} BufferContext;

void delete_player(gpointer data) {
    Player* p = (Player*) data;
    g_free(p);
}

void delete_lobby(gpointer data) {
    Lobby* lobby = (Lobby*) data;
    pthread_mutex_lock(&(lobby->players_mutex));
    g_slist_free(lobby->players);
    pthread_mutex_unlock(&(lobby->players_mutex));
    g_free(lobby);
}

void fill_buffer(gpointer key, gpointer value, gpointer bufferContext) {
    Lobby* lobby = (Lobby*) value;
    BufferContext* bufCont = (BufferContext*) bufferContext;
    int written = snprintf(bufCont->buffer + bufCont->idx, 1024, "%s %s %d %d\n",
                          lobby->id,
                          lobby->host->username,
                          lobby->max_players,
                          (int)g_slist_length(lobby->players));
    bufCont->idx += written;
}

void lobby_broadcast_disconnection(gpointer player, gpointer ssender) {
    Player* p = (Player*) player;
    Player* sender = (Player*) ssender;
    if (strcmp(p->id, sender->id) == 0) {
        return; //do not send to sender
    }

    char* message = (sender->isHost) ? 
        "L'host ha abbandonato la lobby, sei stato disconesso" :
        "Un player ha abbandonato la lobby";
    pthread_mutex_lock(&(p->socket_mutex));
    printf("sto inviando il messaggio a %s\n", p->username);
    send(p->socket, message, strlen(message), 0);
    pthread_mutex_unlock(&(p->socket_mutex));
    p->lobby = NULL;
}

GHashTable* players;
GHashTable* lobbies;

pthread_mutex_t lobbies_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_players_mutex = PTHREAD_MUTEX_INITIALIZER;

void print_lobby(const Lobby* lobby){
    printf("Lobby:id %s \n",lobby->id);
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[1024];
    int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
    buffer[bytes] = '\0';
    printf("Nuovo player: %s\n", buffer);

    Player *p = g_new(Player, 1);
    uuid_t id;
    uuid_generate_random(id);
    uuid_unparse(id, p->id);
    strcpy(p->username, buffer);
    p->socket = client_socket;
    p->lobby = NULL;
    p->isHost = false;
    pthread_mutex_init(&(p->socket_mutex), NULL);
    pthread_mutex_lock(&global_players_mutex);
    g_hash_table_insert(players, g_strdup(p->id), p);
    pthread_mutex_unlock(&global_players_mutex);

    sprintf(buffer, "Benvenuto,! Il tuo username è %s\n", p->username);
    pthread_mutex_lock(&(p->socket_mutex));
    send(client_socket, buffer, strlen(buffer), 0);
    pthread_mutex_unlock(&(p->socket_mutex));
  
    while (1)
    {
        int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0)
            break;
        buffer[bytes] = '\0';  
        char op[4];
        printf("Player %s (%s): %s\n", p->username, p->id, buffer);
        strncpy(op,buffer,3);
        int op_number=atoi(op);
        switch (op_number)
        {
            case OP_CREATE_LOBBY: {
                if(p->lobby){
                    char error_messagge[] = "Non puoi creare la lobby perche gia sei in una lobby";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if(g_hash_table_size(lobbies) + 1 > MAX_LOBBIES){
                    char error_messagge[] = "Non puoi creare la lobby perche abbiamo finito i posti!";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket,error_messagge,sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                Lobby *lobby = g_new(Lobby, 1);
                uuid_t id;
                uuid_generate_random(id);
                uuid_unparse(id, lobby->id);
                pthread_mutex_init(&(lobby->players_mutex), NULL);
                lobby->host = p;
                lobby->max_players = MAX_PLAYERS; //TODO setta il max_players passato dal client
                p->lobby = lobby;
                p->isHost = true;
                lobby->players = NULL;
                lobby->players = g_slist_append(lobby->players, lobby->host);
                pthread_mutex_lock(&lobbies_mutex);
                g_hash_table_insert(lobbies, g_strdup(lobby->id), lobby);
                pthread_mutex_unlock(&lobbies_mutex);
                char success_lobby[] = "Hai creato la lobby con successo!";
                print_lobby(lobby);
                printf("Lobby created, number of lobbies: %d\n", g_hash_table_size(lobbies));
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket,success_lobby,sizeof(success_lobby),0);
                pthread_mutex_unlock(&(p->socket_mutex));
                break;
            }
            case OP_JOIN_LOBBY : {
                char lobby_id[37];
                strncpy(lobby_id,buffer+4,36);
                lobby_id[36]='\0';
                if(p->lobby){
                    char error_messagge[] = "Non puoi unirti alla lobby perche gia sei in una lobby";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }

                Lobby *lobby = (Lobby *) g_hash_table_lookup(lobbies, lobby_id);
                if(!lobby){
                    char error_messagge[] = "Non abbiamo trovato la lobby!";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if (g_slist_length(lobby->players) + 1 > lobby->max_players) {
                    char error_messagge[] = "Non puoi unirti alla lobby perche e piena";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }

                pthread_mutex_lock(&(lobby->players_mutex));
                lobby->players = g_slist_append(lobby->players, p);
                p->lobby = lobby;
                pthread_mutex_unlock(&(lobby->players_mutex));
                
                char response_message[] = "Benvenuto in lobby";
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, response_message, sizeof(response_message), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
                break;
            }
            case OP_GET_LOBBIES: {
                if (g_hash_table_size(lobbies) <= 0) {
                    char error_messagge[] = "Non esistono lobby al momento";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                printf("Number of lobbies to show: %d\n", g_hash_table_size(lobbies));
                char* buffer = malloc(g_hash_table_size(lobbies) * 200 + 1);  // ~200 char per lobby
                BufferContext bufferContext = {buffer, 0};
                g_hash_table_foreach(lobbies, fill_buffer, &bufferContext);  
                buffer[bufferContext.idx] = '\0';
                printf("Buffer to send is:\n%s\n", buffer);
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, buffer, strlen(buffer), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
                free(buffer);
                break;
            }
            case OP_LEAVE_LOBBY: {
                if (!p->lobby) {
                    char error_messagge[] = "Non fai parte di una lobby";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if (p->isHost) {
                    g_slist_foreach(p->lobby->players, lobby_broadcast_disconnection, p);
                    pthread_mutex_lock(&lobbies_mutex);
                    g_hash_table_remove(lobbies, p->lobby->id);
                    pthread_mutex_unlock(&lobbies_mutex);
                    p->lobby = NULL;
                } else {
                    g_slist_foreach(p->lobby->players, lobby_broadcast_disconnection, p);
                    pthread_mutex_lock(&(p->lobby->players_mutex));
                    p->lobby->players = g_slist_remove(p->lobby->players, p);
                    pthread_mutex_unlock(&(p->lobby->players_mutex));
                    p->lobby = NULL;
                }
                break;
            }
            default:{
                char default_message[] = "Non ho capito il tuo messaggio";
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, default_message, sizeof(default_message), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
            }
        }
    }

    close(client_socket);
    printf("Player %s (%s) disconnesso.\n", p->username, p->id);

    if (p->lobby) {
        g_slist_foreach(p->lobby->players, lobby_broadcast_disconnection, p);
        if (p->isHost) {
            pthread_mutex_lock(&lobbies_mutex);
            g_hash_table_remove(lobbies, p->lobby->id);
            pthread_mutex_unlock(&lobbies_mutex);
        }
        p->lobby = NULL;
    }

    pthread_mutex_lock(&global_players_mutex);
    g_hash_table_remove(players, p->id);
    pthread_mutex_unlock(&global_players_mutex);

    pthread_exit(NULL);
}

int main()
{
    players = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, delete_player);
    lobbies = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, delete_lobby);

    int server_fd, new_socket, *client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    listen(server_fd, 3);

    printf("Server in ascolto su porta %d\n", PORT);

    while (1)
    {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        client_socket = malloc(sizeof(int));
        *client_socket = new_socket;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_socket);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
