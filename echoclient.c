/*
 * echoclient.c - An echo client
 */
/* $begin echoclientmain */
#include "constants.h"
#include "csapp.h"

int main(int argc, char **argv) {
    int clientfd;
    char *host, *port /*, buf[MAXLINE]*/;
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    size_t n;
    char input[2];
    TILE grid[GRIDSIZE][GRIDSIZE];
    Position playerPositions[MAX_CLIENTS];
    int player;
    n = Rio_readnb(&rio, &player, sizeof(player));  // first thing the client sends is the player number
    printf("we are player %d\n", player);

    while ((n = Rio_readnb(&rio, &input, 2)) != 0) {
        // printf("server received %d bytes -> input: %c \n", (int)n, input);
        if (input[0] == 'G') {  // next thing we will receive is a grid
            printf("receved grid object \n");
            Rio_readnb(&rio, &grid, sizeof(grid));
            for (int i = 0; i < GRIDSIZE; i++) {
                printf("[ ");
                for (int j = 0; j < GRIDSIZE; j++) {
                    printf("%d, ", grid[i][j]);
                }
                printf("]\n");
            }
        } else {  // we received a 'P' (POSITIONS)
            printf("recieved Positiosn array\n");
            Rio_readnb(&rio, &playerPositions, sizeof(playerPositions));
            for (int i = 0; i < MAX_CLIENTS; i++) {
                printf("Player %d is at (%d, %d)\n", playerPositions[i].client, playerPositions[i].x, playerPositions[i].y);
            }
        }

        // Rio_writen(connfd, buf, n);
    }
    Close(clientfd);  // line:netp:echoclient:close
    exit(0);
}
/* $end echoclientmain */