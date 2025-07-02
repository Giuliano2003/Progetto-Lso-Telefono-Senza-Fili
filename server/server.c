#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <glib-2.0/glib.h>
#include "translator.h"

#define PORT 8080
#define MIN_PLAYERS 2
#define MAX_PLAYERS 10
#define MAX_LOBBIES 5
#define MAX_LENGTH 30

#define OP_CREATE_LOBBY 100
#define OP_JOIN_LOBBY 101
#define OP_GET_LOBBIES 102
#define OP_LEAVE_LOBBY 103
#define OP_START_MATCH 110
#define OP_SPEAK 111
#define OP_ANON_LOGIN 200

const char* translator_url = "http://libretranslate:5000/translate";

typedef struct Lobby Lobby;
typedef struct Match Match;

typedef struct
{
    char id[37];
    char username[32];
    char language[3];
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
    GList* players;
    GQueue* queue;
    Match* match;
    Translator* translator;
    pthread_mutex_t players_mutex;
};

typedef struct {
    char id[37];
    char host_username[32];
    int max_players;
    int player_count;
} LobbyDto;

struct Match {
    int turn;
    bool clockwise;
    bool terminated;
    GSList* word;
};

typedef struct {
    char* buffer;
    int idx; //chars written
} BufferContext;

typedef struct {
    Player* player_turn;
    bool terminated;
    GSList* word;
} TurnContext;

void delete_player(gpointer data) {
    Player* p = (Player*) data;
    g_free(p);
}

void delete_lobby(gpointer data) {
    Lobby* lobby = (Lobby*) data;
    pthread_mutex_lock(&(lobby->players_mutex));
    g_list_free(lobby->players);
    g_queue_free(lobby->queue);
    pthread_mutex_unlock(&(lobby->players_mutex));
    free(lobby->match);
    free(lobby->translator);
    g_free(lobby);
}

void fill_buffer(gpointer key, gpointer value, gpointer bufferContext) {
    Lobby* lobby = (Lobby*) value;
    BufferContext* bufCont = (BufferContext*) bufferContext;
    int written = snprintf(bufCont->buffer + bufCont->idx, 1024, "%s %s %d %d\n",
                          lobby->id,
                          lobby->host->username,
                          lobby->max_players,
                          (int)g_list_length(lobby->players));
    bufCont->idx += written;
}

void lobby_broadcast_disconnection(gpointer player, gpointer ssender) {
    Player* p = (Player*) player;
    Player* sender = (Player*) ssender;
    if (strcmp(p->id, sender->id) == 0) {
        return; //do not send to sender
    }

    char* message = (sender->isHost) ? 
        "300\nThe host left, leaving the lobby" :
        "301\nA player left the lobby";
    pthread_mutex_lock(&(p->socket_mutex));
    printf("sto inviando il messaggio a %s\n", p->username);
    send(p->socket, message, strlen(message), 0);
    pthread_mutex_unlock(&(p->socket_mutex));
    if(sender->isHost){
        p->lobby = NULL;
    } else {
        if (!p->lobby->match->terminated) {
            char * match_terminated = "200\nThe match is terminated";
            pthread_mutex_lock(&(p->socket_mutex));
            printf("sto inviando il messaggio a %s\n", p->username);
            send(p->socket, match_terminated, strlen(match_terminated), 0);
            pthread_mutex_unlock(&(p->socket_mutex));
        }
    }
}

void word_history(gpointer word_step, gpointer bufferContext) {
    char* step = (char*) word_step;
    BufferContext* context = (BufferContext*) bufferContext;
    
    int written = snprintf(context->buffer + context->idx, strlen(step)+5, "%s -> ", step);
    context->idx += written;
}

void match_turn_broadcast(gpointer player, gpointer turnContext) {
    Player* p = (Player*) player;
    TurnContext* context = (TurnContext*) turnContext;

    char message[MAX_LENGTH*MAX_PLAYERS+200] = {0};
    char body[MAX_LENGTH*MAX_PLAYERS+100] = {0};
    if (context->terminated) {
        const char* header = "304\nThe match is terminated\nHere is the story of the phrase:\n";
        strcpy(body, header);
        int header_len = strlen(header);

        char phrase[MAX_LENGTH*MAX_PLAYERS+50] = {0};
        int phrase_idx = 0;
        for (GSList* node = context->word; node != NULL; node = node->next) {
            char* step = (char*) node->data;
            int written = snprintf(phrase + phrase_idx, sizeof(phrase) - phrase_idx, "%s -> ", step);
            phrase_idx += written;
        }
        if (phrase_idx >= 4) {
            phrase[phrase_idx-4] = '\0';
        }
        snprintf(body + header_len, sizeof(body) - header_len, "%s\n", phrase);
    } else {
        if (p->id == context->player_turn->id) {
            GSList* node = g_slist_last(context->word);
            if (node) {
                snprintf(body, sizeof(body), "302\nIs your turn!\nThe current phrase is: %s\n", (char*) node->data);
            } else {
                snprintf(body, sizeof(body), "302\nIs your turn!\nStart with a phrase\n");
            }
        } else {
            snprintf(body, sizeof(body), "303\nWait for the other players to finish");
        }
    }
    strncat(message, body, sizeof(message) - strlen(message) - 1);
    pthread_mutex_lock(&(p->socket_mutex));
    printf("\nSto mandando questo messaggio: %s\n", message);
    send(p->socket, message, strlen(message), 0);
    pthread_mutex_unlock(&(p->socket_mutex));
}

GHashTable* players;
GHashTable* lobbies;

pthread_mutex_t lobbies_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_players_mutex = PTHREAD_MUTEX_INITIALIZER;

void print_lobby(const Lobby* lobby){
    printf("Lobby:id %s \n",lobby->id);
}

void sanitize_username(char *buffer) {
    int read_pos = 0;
    int write_pos = 0;
    while (buffer[read_pos] != '\0') {
        if (isalnum(buffer[read_pos])) {
            buffer[write_pos] = buffer[read_pos];
            write_pos++;
        }
        read_pos++;
    }
    buffer[write_pos] = '\0';
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[1024];
    Player *p = NULL;
    while (1)
    {
        int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0)
            break;
        buffer[bytes] = '\0';
        char op[4];
        strncpy(op,buffer,3);
        op[3] = '\0';
        int op_number=atoi(op);
        if(p){
            printf("Player %s (%s): %s\n", p->username, p->id, buffer);
        }
        switch (op_number)
        {
            case OP_ANON_LOGIN: {
                if (p) {
                    char * msg = "400\nYou are already authenticated!";
                    send(client_socket, msg, strlen(msg), 0);
                    break;
                }
                sanitize_username(buffer+7);
                if (strlen(buffer+7) < 5) {
                    char * msg = "400\nUsername too short (min: 5)";
                    send(client_socket, msg, strlen(msg), 0);
                    break;
                }
                if (strlen(buffer+7) > 15) {
                    char * msg = "400\nUsername too long (max: 15)";
                    send(client_socket, msg, strlen(msg), 0);
                    break;
                }
                p = g_new(Player, 1);
                uuid_t id;
                uuid_generate_random(id);
                uuid_unparse(id, p->id);
                strcpy(p->username, buffer+7);
                p->socket = client_socket;
                p->lobby = NULL;
                p->isHost = false;
                strncpy(p->language, buffer+4, 2);
                p->language[2] = '\0';
                pthread_mutex_init(&(p->socket_mutex), NULL);
                pthread_mutex_lock(&global_players_mutex);
                g_hash_table_insert(players, g_strdup(p->id), p);
                pthread_mutex_unlock(&global_players_mutex);

                sprintf(buffer, "Welcome! Your username is %s\n", p->username);
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, buffer, strlen(buffer), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
                printf("New Player %s (%s) --> %s\n", p->username, p->id, p->language);
                break;
            }
            case OP_CREATE_LOBBY: {

                if (!p) {
                    char * msg = "400\nYou must authenticate first!";
                    send(client_socket, msg, strlen(msg), 0);
                }
                if(p->lobby){
                    char error_messagge[] = "400\nYou cannot create a lobby since you already are in one";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if(g_hash_table_size(lobbies) + 1 > MAX_LOBBIES){
                    char error_messagge[] = "500\nWe have not room for other lobbies at the moment. Try later!";
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
                lobby->max_players = MAX_PLAYERS;
                lobby->queue = g_queue_new();
                p->lobby = lobby;
                p->isHost = true;
                lobby->players = NULL;
                lobby->match = malloc(sizeof(Match));
                lobby->match->terminated = true;
                lobby->translator = malloc(sizeof(Translator));
                translator_init(lobby->translator, translator_url);
                lobby->players = g_list_append(lobby->players, lobby->host);
                pthread_mutex_lock(&lobbies_mutex);
                g_hash_table_insert(lobbies, g_strdup(lobby->id), lobby);
                pthread_mutex_unlock(&lobbies_mutex);
                char success_message[] = "Lobby created!";
                print_lobby(lobby);
                printf("Lobby created, number of lobbies: %d\n", g_hash_table_size(lobbies));
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, success_message, sizeof(success_message), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
                break;
            }
            case OP_JOIN_LOBBY : {
                if (!p) {
                    char * msg = "400\nYou must authenticate first!";
                    send(client_socket, msg, strlen(msg), 0);
                }
                char lobby_id[37];
                strncpy(lobby_id,buffer+4,36);
                lobby_id[36]='\0';
                if(p->lobby){
                    char error_messagge[] = "400\nYou are already in a lobby";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }

                Lobby *lobby = (Lobby *) g_hash_table_lookup(lobbies, lobby_id);
                if(!lobby){
                    char error_messagge[] = "404\nLobby not found";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if (!lobby->match->terminated) {
                    char error_messagge[] = "400\nThe match is already started, you are in a queue now";
                    pthread_mutex_lock(&(lobby->players_mutex));
                    g_queue_push_tail(lobby->queue, p);
                    p->lobby = lobby;
                    pthread_mutex_unlock(&(lobby->players_mutex));
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if (g_list_length(lobby->players) + 1 > lobby->max_players) {
                    char error_messagge[] = "400\nThe lobby is full, you are in a queue now";
                    pthread_mutex_lock(&(lobby->players_mutex));
                    g_queue_push_tail(lobby->queue, p);
                    p->lobby = lobby;
                    pthread_mutex_unlock(&(lobby->players_mutex));
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }

                pthread_mutex_lock(&(lobby->players_mutex));
                lobby->players = g_list_append(lobby->players, p);
                p->lobby = lobby;
                pthread_mutex_unlock(&(lobby->players_mutex));
                
                char response_message[] = "200\nWelcome to the lobby";
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, response_message, sizeof(response_message), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
                break;
            }
            case OP_GET_LOBBIES: {
                if (!p) {
                    char * msg = "400\nYou must authenticate first!";
                    send(client_socket, msg, strlen(msg), 0);
                    break;
                }
                if (g_hash_table_size(lobbies) <= 0) {
                    printf("Non ci sono lobby !\n");
                    char error_messagge[] = "";
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
                if (!p) {
                    char * msg = "400\nYou must authenticate first!";
                    send(client_socket, msg, strlen(msg), 0);
                    break;
                }
                if (!p->lobby) {
                    char error_messagge[] = "400\nYou are not in a lobby";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if (p->isHost) {
                    g_list_foreach(p->lobby->players, lobby_broadcast_disconnection, p);
                    g_queue_foreach(p->lobby->queue, lobby_broadcast_disconnection, p);
                    pthread_mutex_lock(&lobbies_mutex);
                    g_hash_table_remove(lobbies, p->lobby->id);
                    pthread_mutex_unlock(&lobbies_mutex);
                    p->lobby = NULL;
                    p->isHost = false;
                } else {
                    if (g_queue_find(p->lobby->queue, p)){
                        pthread_mutex_lock(&(p->lobby->players_mutex));
                        g_queue_pop_head(p->lobby->queue);
                        pthread_mutex_unlock(&(p->lobby->players_mutex));
                        char success_message[] = "200\nYou left the queue";
                        pthread_mutex_lock(&(p->socket_mutex));
                        send(p->socket, success_message, sizeof(success_message), 0);
                        pthread_mutex_unlock(&(p->socket_mutex));
                        p->lobby = NULL;
                    }
                    else
                    {
                        g_list_foreach(p->lobby->players, lobby_broadcast_disconnection, p);
                        p->lobby->match->terminated = true;
                        pthread_mutex_lock(&(p->lobby->players_mutex));
                        p->lobby->players = g_list_remove(p->lobby->players, p);
                        if (!g_queue_is_empty(p->lobby->queue)){
                            Player *queue_player = g_queue_pop_head(p->lobby->queue);
                            p->lobby->players = g_list_append(p->lobby->players, queue_player);
                            char success_message[] = "200\nWelcome to the lobby";
                            pthread_mutex_lock(&(queue_player->socket_mutex));
                            send(queue_player->socket, success_message, sizeof(success_message), 0);
                            pthread_mutex_unlock(&(queue_player->socket_mutex));
                        }
                        pthread_mutex_unlock(&(p->lobby->players_mutex));
                        p->lobby = NULL;
                    }
                }
                char success_message[] = "200\nYou left the lobby";
                pthread_mutex_lock(&(p->socket_mutex));
                send(p->socket, success_message, sizeof(success_message), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
                break;
            }
            case OP_START_MATCH: {
                if (!p) {
                    char * msg = "400\nYou must authenticate first!";
                    send(client_socket, msg, strlen(msg), 0);
                    break;
                }
                if (!p->isHost) {
                    char error_messagge[] = "400\nYou are not the host";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if (g_list_length(p->lobby->players) < MIN_PLAYERS) {
                    char error_messagge[] = "400\nMinimum 4 players required";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                char clockwise[1];
                strncpy(clockwise,buffer+4,1);
                clockwise[1]='\0';
                Match* match = p->lobby->match;
                match->turn = 0;
                match->terminated = false;
                match->word = NULL;
                match->clockwise = (clockwise[0] != '0');
                if (!match->clockwise) {
                    pthread_mutex_lock(&(p->lobby->players_mutex));
                    p->lobby->players = g_list_reverse(p->lobby->players);
                    GList* last = g_list_last(p->lobby->players);
                    p->lobby->players = g_list_delete_link(p->lobby->players, last);
                    p->lobby->players = g_list_prepend(p->lobby->players, p->lobby->host);
                    pthread_mutex_unlock(&(p->lobby->players_mutex));
                }
                TurnContext context = {p, false, NULL};
                g_list_foreach(p->lobby->players, match_turn_broadcast, &context);
                break;
            }
            case OP_SPEAK: {
                if (!p) {
                    char * msg = "400\nYou must authenticate first!";
                    send(client_socket, msg, strlen(msg), 0);
                    break;
                }
                if (!p->lobby) {
                    char error_messagge[] = "400\nYou are not in a lobby";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                if (p->lobby->match->terminated) {
                    char error_messagge[] = "400\nThe match is terminated";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                GList* player_node = g_list_nth(p->lobby->players, p->lobby->match->turn);
                if (p->id != ((Player*) player_node->data)->id) {
                    char error_messagge[] = "400\nIs not your turn";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }

                char word_len[3];
                strncpy(word_len, buffer+4, 2);
                word_len[2] = '\0';
                int len = atoi(word_len);
                printf("Inserted word length is %d\n", len);

                if (len >= MAX_LENGTH) {
                    char error_messagge[] = "400\nThe maximum length is 30";
                    pthread_mutex_lock(&(p->socket_mutex));
                    send(client_socket, error_messagge, sizeof(error_messagge), 0);
                    pthread_mutex_unlock(&(p->socket_mutex));
                    break;
                }
                char* word = malloc(len*sizeof(char)+1);
                strncpy(word, buffer+7, len);
                word[len] = '\0';
                
                printf("The parsed word is: %s\n", word);
                char* translated_phrase = malloc(MAX_PLAYERS*(MAX_LENGTH + 1));

                GList* nextNode = player_node->next;
                Player* nextPlayer = NULL;
                if (nextNode) {
                    nextPlayer = (Player*) nextNode->data;
                } else {
                    nextPlayer = (Player*) p->lobby->players->data;
                }

                if (p->lobby->match->turn == 0) {
                    p->lobby->match->word = g_slist_append(p->lobby->match->word, word);
                    if (translate(p->lobby->translator, word, p->language, nextPlayer->language, translated_phrase, MAX_PLAYERS*(MAX_LENGTH+1)) == 0) {
                        p->lobby->match->word = g_slist_append(p->lobby->match->word, translated_phrase);
                    } else {
                        p->lobby->match->word = g_slist_append(p->lobby->match->word, word);
                        free(translated_phrase);
                    }
                } else {
                    GSList* prev_node = g_slist_last(p->lobby->match->word);
                    char* current_phrase = malloc(MAX_LENGTH*MAX_PLAYERS);
                    printf("The concatenation is %s %s\n", (char*) prev_node->data, word);
                    snprintf(current_phrase, MAX_LENGTH*MAX_PLAYERS, "%s %s", (char*) prev_node->data, word);
                    free(prev_node->data);
                    prev_node->data = current_phrase;
                    printf("The current phrase is %s\n", (char*) prev_node->data);
                    if (nextNode) {
                        if (translate(p->lobby->translator, (char*) prev_node->data, p->language, nextPlayer->language, translated_phrase, MAX_PLAYERS*(MAX_LENGTH+1)) == 0) {
                            p->lobby->match->word = g_slist_append(p->lobby->match->word, translated_phrase);
                        } else {
                            p->lobby->match->word = g_slist_append(p->lobby->match->word, (char*) prev_node->data);
                            free(translated_phrase);
                        }
                    }
                }
                p->lobby->match->turn++;
                if(p->lobby->match->turn >= g_list_length(p->lobby->players)) {
                    p->lobby->match->terminated = true;
                }
                TurnContext context = {nextPlayer, p->lobby->match->terminated, p->lobby->match->word};
                g_list_foreach(p->lobby->players, match_turn_broadcast, &context);
                if (p->lobby->match->terminated) {
                    pthread_mutex_lock(&(p->lobby->players_mutex));
                    while (g_list_length(p->lobby->players) < p->lobby->max_players && !g_queue_is_empty(p->lobby->queue)) {
                        Player* queue_player = (Player*) g_queue_pop_head(p->lobby->queue);
                        p->lobby->players = g_list_append(p->lobby->players, queue_player);
                        char success_message[] = "200\nWelcome to the lobby";
                        pthread_mutex_lock(&(queue_player->socket_mutex));
                        send(queue_player->socket, success_message, sizeof(success_message), 0);
                        pthread_mutex_unlock(&(queue_player->socket_mutex));
                    }
                    pthread_mutex_unlock(&(p->lobby->players_mutex));
                }
                break;
            }
            default: {
                if (!p) {
                    char * msg = "400\nYou must authenticate first!";
                    send(client_socket, msg, strlen(msg), 0);
                    break;
                }
                char default_message[] = "500\nUnknown request";
                pthread_mutex_lock(&(p->socket_mutex));
                send(client_socket, default_message, sizeof(default_message), 0);
                pthread_mutex_unlock(&(p->socket_mutex));
            }
        }
    }

    close(client_socket);
    printf("Player %s (%s) disconnesso.\n", p->username, p->id);

    if (p->lobby) {
        if (p->isHost) {
            g_list_foreach(p->lobby->players, lobby_broadcast_disconnection, p);
            g_queue_foreach(p->lobby->queue, lobby_broadcast_disconnection, p);
            pthread_mutex_lock(&lobbies_mutex);
            g_hash_table_remove(lobbies, p->lobby->id);
            pthread_mutex_unlock(&lobbies_mutex);
            p->lobby = NULL;
            p->isHost = false;
        } else {
            if (g_queue_find(p->lobby->queue, p)){
                pthread_mutex_lock(&(p->lobby->players_mutex));
                g_queue_pop_head(p->lobby->queue);
                pthread_mutex_unlock(&(p->lobby->players_mutex));
                p->lobby = NULL;
            }
            else
            {
                g_list_foreach(p->lobby->players, lobby_broadcast_disconnection, p);
                p->lobby->match->terminated = true;
                pthread_mutex_lock(&(p->lobby->players_mutex));
                p->lobby->players = g_list_remove(p->lobby->players, p);
                if (!g_queue_is_empty(p->lobby->queue)){
                    Player *queue_player = g_queue_pop_head(p->lobby->queue);
                    p->lobby->players = g_list_append(p->lobby->players, queue_player);
                    char success_message[] = "200\nWelcome to the lobby";
                    pthread_mutex_lock(&(queue_player->socket_mutex));
                    send(queue_player->socket, success_message, sizeof(success_message), 0);
                    pthread_mutex_unlock(&(queue_player->socket_mutex));
                }
                pthread_mutex_unlock(&(p->lobby->players_mutex));
                p->lobby = NULL;
            }
        }
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
