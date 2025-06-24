#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main()
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    printf("Inserisci username: ");
    fgets(buffer, sizeof(buffer), stdin);
    send(sock, buffer, strlen(buffer), 0);

    int bytes = recv(sock, buffer, sizeof(buffer), 0);
    buffer[bytes] = '\0';
    printf("Server: %s\n", buffer);

    while (1)
    {
        printf("> ");
        fgets(buffer, sizeof(buffer), stdin);
        send(sock, buffer, strlen(buffer), 0);
        bytes = recv(sock, buffer, sizeof(buffer), 0);
        buffer[bytes] = '\0';
        printf("Server: %s\n", buffer);
    }

    close(sock);
    return 0;
}
