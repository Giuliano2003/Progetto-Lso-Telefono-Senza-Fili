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


typedef struct
{
    uuid_t id;
    char username[32];
    int socket;
    bool isInLobby;
} Player;

typedef struct
{
    uuid_t id;
    Player *host;
    int max_players;
    Player *players[MAX_PLAYERS];
    int player_count;
} Lobby;

Player *players[MAX_PLAYERS];
Lobby *lobbies[MAX_LOBBIES];
int global_player_count = 0;
int lobby_count = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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
    char response_buffer[1024];
    recv(client_socket, buffer, sizeof(buffer), 0);
    printf("Nuovo player: %s\n", buffer);

    pthread_mutex_lock(&lock);
    Player *p = malloc(sizeof(Player));
    uuid_generate_random(p->id);
    strcpy(p->username, buffer);
    p->socket = client_socket;
    p->isInLobby = false;
    players[global_player_count++] = p;
    pthread_mutex_unlock(&lock);

    sprintf(buffer, "Benvenuto,! Il tuo username è %s\n", p->username);
    send(client_socket, buffer, strlen(buffer), 0);

  
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
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    break;
                }
                if(lobby_count + 1 > MAX_LOBBIES){
                    char error_messagge[] = "Non puoi creare la lobby perche abbiamo finito i posti!";
                    send(client_socket,error_messagge,sizeof(error_messagge), 0);
                    break;
                }
                Lobby *lobby = malloc(sizeof(Lobby));
                uuid_generate_random(lobby->id);
                lobby->host = p;
                p->isInLobby = true;
                lobby->players[lobby->player_count] = lobby->host; //setto host a 0
                lobby->player_count++; // aumento
                pthread_mutex_lock(&lock);
                lobbies[lobby_count] = lobby;
                lobby_count++;
                pthread_mutex_unlock(&lock);
                char success_lobby[] = "Hai creato la lobby con successo!";
                print_lobby(lobby);
                send(client_socket,success_lobby,sizeof(success_lobby),0);
                break;
            }
            case OP_JOIN_LOBBY : {
                char lobby_id[37];
                uuid_t lobby_uuid;
                strncpy(lobby_id,buffer+4,36);
                lobby_id[36]='\0';
                if(p->isInLobby){
                    char error_messagge[] = "Non puoi unirti alla lobby perche gia sei in una lobby";
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    break;
                }
                uuid_parse(lobby_id,lobby_uuid);
                Lobby *lobby = find_lobby(lobby_uuid);
                if(!lobby){
                    char error_messagge[] = "Non abbiamo trovato la lobby!";
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    break;
                }
                //TO-DO 1)Vedere se il max players supera il limite; 2)Aggionare max players
                lobby->players[lobby->player_count] = p;
            
                break;
            }
            default:{
                char default_messagge[] = "Non ho capito il tuo messaggio";
                send(client_socket,default_messagge,sizeof(default_messagge),0);
            }
        }
    }

    close(client_socket);
    printf("Player %s disconnesso.\n", p->username);

    pthread_mutex_lock(&lock);
    free(p);
    pthread_mutex_unlock(&lock);

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
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
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
