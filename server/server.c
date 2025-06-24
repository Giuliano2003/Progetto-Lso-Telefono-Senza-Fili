#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>

#define PORT 8080
#define MAX_PLAYERS 10
#define MAX_LOBBIES 5

#define OP_CREATE_LOBBY 100
#define OP_JOIN_LOBBY 101
#define OP_GET_LOBBIES 102

typedef struct Lobby Lobby;

typedef struct
{
    uuid_t id;
    char username[32];
    int socket;
    bool isInLobby;
    Lobby* lobby_hosted;
    pthread_mutex_t socket_mutex;
} Player;

struct Lobby
{
    uuid_t id;
    Player *host;
    int max_players;
    Player *players[MAX_PLAYERS];
    int player_count;
    pthread_mutex_t players_mutex;
};

typedef struct {
    char id[37];
    char host_username[32];
    int max_players;
    int player_count;
} LobbyDto;

//TODO sostituisci con hashmap
Player *players[MAX_PLAYERS];
Lobby *lobbies[MAX_LOBBIES];
int global_player_count = 0;
int lobby_count = 0;

pthread_mutex_t lobbies_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_players_mutex = PTHREAD_MUTEX_INITIALIZER;

void print_lobby(const Lobby* lobby){
    char id_str[37];
    uuid_unparse(lobby->id,id_str);
    printf("Lobby:id %s \n",id_str);
}

Lobby* find_lobby(uuid_t id_lobby){
    for (unsigned int i = 0; i < lobby_count; i++)
    {
        if (uuid_compare(lobbies[i]->id,id_lobby) == 0){
            return lobbies[i];
        }
    }
    return NULL;
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[1024];
    recv(client_socket, buffer, sizeof(buffer), 0);
    printf("Nuovo player: %s\n", buffer);

    Player *p = malloc(sizeof(Player));
    uuid_generate_random(p->id);
    strcpy(p->username, buffer);
    p->socket = client_socket;
    p->isInLobby = false;
    p->lobby_hosted = NULL;
    pthread_mutex_init(&(p->socket_mutex), NULL);
    pthread_mutex_lock(&global_players_mutex);
    players[global_player_count++] = p;
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
        printf("Player (%s): %s \n", p->username, buffer);
        strncpy(op,buffer,3);
        int op_number=atoi(op);
        switch (op_number)
        {
            case OP_CREATE_LOBBY: {
                if(p->isInLobby){
                    char error_messagge[] = "Non puoi creare la lobby perche gia sei in una lobby";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if(lobby_count + 1 > MAX_LOBBIES){
                    char error_messagge[] = "Non puoi creare la lobby perche abbiamo finito i posti!";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket,error_messagge,sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                Lobby *lobby = malloc(sizeof(Lobby));
                uuid_generate_random(lobby->id);
                pthread_mutex_init(&(lobby->players_mutex), NULL);
                lobby->host = p;
                lobby->max_players = MAX_PLAYERS; //TODO setta il max_players passato dal client
                p->isInLobby = true;
                p->lobby_hosted = lobby;
                lobby->players[lobby->player_count] = lobby->host; //setto host a 0
                lobby->player_count++; // aumento
                pthread_mutex_lock(&lobbies_mutex);
                lobbies[lobby_count] = lobby;
                lobby_count++;
                pthread_mutex_unlock(&lobbies_mutex);
                char success_lobby[] = "Hai creato la lobby con successo!";
                print_lobby(lobby);
                printf("lobby_count: %d\n", lobby_count);
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket,success_lobby,sizeof(success_lobby),0);
                pthread_mutex_unlock(&(p->socket_mutex));
                break;
            }
            case OP_JOIN_LOBBY : {
                char lobby_id[37];
                uuid_t lobby_uuid;
                strncpy(lobby_id,buffer+4,36);
                lobby_id[36]='\0';
                if(p->isInLobby){
                    char error_messagge[] = "Non puoi unirti alla lobby perche gia sei in una lobby";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                uuid_parse(lobby_id,lobby_uuid);
                Lobby *lobby = find_lobby(lobby_uuid);
                if(!lobby){
                    char error_messagge[] = "Non abbiamo trovato la lobby!";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if (lobby->player_count + 1 > lobby->max_players) {
                    char error_messagge[] = "Non puoi unirti alla lobby perche e piena";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }

                pthread_mutex_lock(&(lobby->players_mutex));
                lobby->players[lobby->player_count++] = p;
                p->isInLobby = true;
                pthread_mutex_unlock(&(lobby->players_mutex));
                
                char response_message[] = "Benvenuto in lobby";
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, response_message, sizeof(response_message), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
                break;
            }
            case OP_GET_LOBBIES: {
                if (lobby_count <= 0) {
                    char error_messagge[] = "Non esistono lobby al momento";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                printf("in OP_GET_LOBBIES lobby_count: %d\n", lobby_count);
                char* buffer = malloc(lobby_count*(sizeof(LobbyDto)+4)+1);
                for (int i = 0; i < lobby_count; i++) {
                    LobbyDto lob;
                    uuid_unparse(lobbies[i]->id, lob.id);
                    strcpy(lob.host_username, lobbies[i]->host->username);
                    lob.player_count = lobbies[i]->player_count;
                    lob.max_players = lobbies[i]->max_players;

                    snprintf(buffer + i*(sizeof(LobbyDto)), sizeof(LobbyDto), "%s %s %d %d\n",
                                lob.id,
                                lob.host_username,
                                lob.max_players,
                                lob.player_count);
                }
                buffer[lobby_count*(sizeof(LobbyDto))] = '\0';
                printf("il contenuto del buffer è:\n%s\n", buffer);
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, buffer, strlen(buffer), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
                free(buffer);
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
    printf("Player %s disconnesso.\n", p->username);

    printf("p->lobby_hosted: %p\n", p->lobby_hosted);
    if (p->lobby_hosted) {
        Lobby* lobby = p->lobby_hosted;
        
        printf("lobby->player_count: %d\n", lobby->player_count);
        for (int i = 1; i < lobby->player_count; i++) {
            Player* p = lobby->players[i];
            char message[] = "L'host ha abbandonato la lobby, sei stato disconesso";
            pthread_mutex_lock(&(p->socket_mutex));
            printf("sto inviando il messaggio a %s\n", p->username);
            send(p->socket, message, sizeof(message), 0);
            pthread_mutex_unlock(&(p->socket_mutex));
            p->isInLobby = false;
        }
        free(lobby);
        pthread_mutex_lock(&lobbies_mutex);
        lobby_count--;
        pthread_mutex_unlock(&lobbies_mutex);
        //TODO rimuovi lobby->id da lobbies
        lobby = NULL;  
    }
    
    //TODO dealloca player
    pthread_mutex_lock(&global_players_mutex);
    global_player_count--;
    pthread_mutex_unlock(&global_players_mutex);

    pthread_mutex_lock(&lobbies_mutex);
    free(p);
    pthread_mutex_unlock(&lobbies_mutex);

    pthread_exit(NULL);
}

int main()
{
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
