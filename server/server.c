#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../constants.h"
#include "../csapp.h"
#include "../queue.h"

// keeps track of the number of clients we have conncted to the server
int numPlayers = 0;
MoveQueue eventQueue;
pthread_mutex_t lock;
// stroes the fds of each socket connected to the server
int sockets[MAX_CLIENTS];

void echo(int connfd) {
    size_t n;
    int buf[MAXLINE];
    rio_t rio;
    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %d bytes: value in buffer \n", (int)n);

        // Rio_writen(connfd, buf, n);
    }
}

void* clientThread(void* vargp) {
    int connfd = *((int*)vargp);
    Pthread_detach(pthread_self());
    int player = EMPTY_SOCKET;
    Free(vargp);
    printf("trying lock\n");
    int l;
    while ((l = pthread_mutex_lock(&lock)) != 0) {
        ;
    }
    printf("lock obtained\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {  // add the fd for the socket into the sockets array, put it in the first empty (NULL) spot in the array
        if (sockets[i] == EMPTY_SOCKET) {
            sockets[i] = connfd;
            player = i;
            break;
        }
    }
    if (player != -1) {
        numPlayers++;
        sockets[player] = connfd;
    }
    printf("sockets array: \n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        printf("sockets[%d] = %d \n", i, sockets[i]);
    }
    pthread_mutex_unlock(&lock);
    if (player != -1) {
        echo(connfd);
    }
    Close(connfd);
    pthread_mutex_lock(&lock);
    numPlayers--;
    sockets[player] = EMPTY_SOCKET;
    pthread_mutex_unlock(&lock);
    return NULL;
}

// get a random value in the range [0, 1]
double rand01() {
    return (double)rand() / (double)RAND_MAX;
}

// get a random int from [0,9]
int random10() {
    return rand() % 10;
}

int initGrid(TILE grid[10][10], Position playerPositions[MAX_CLIENTS]) {
    int numTomatoes = 0;
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            double r = rand01();
            if (r < 0.1) {
                grid[i][j] = TILE_TOMATO;
                numTomatoes++;
            } else
                grid[i][j] = TILE_GRASS;
        }
    }

    // force player's position to be grass
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int x = playerPositions[i].x;
        int y = playerPositions[i].y;
        if (x == OFF_BOARD && y == OFF_BOARD) {
            continue;
        }
        if (grid[x][y] == TILE_TOMATO) {
            grid[x][y] = TILE_GRASS;
            numTomatoes--;
        }
    }

    // ensure grid isn't empty
    while (numTomatoes == 0)
        initGrid(grid, playerPositions);
    return numTomatoes;
}

void initPlayerPositions(Position playerPostions[MAX_CLIENTS]) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sockets[i] != EMPTY_SOCKET) {
            int x = random10();
            int y = random10();
            playerPostions[i].x = x;
            playerPostions[i].y = y;
            for (int j = 0; j < i; j++) {
                if (x == playerPostions[j].x && y == playerPostions[j].y) {
                    i--;  // reset this player's random position bc its the same as another players random position
                    break;
                }
            }

        } else {  // if no connection to that player, set the position to -1,-1
            printf("sockets[%d] = %d \n", i, sockets[i]);
            playerPostions[i].x = OFF_BOARD;
            playerPostions[i].y = OFF_BOARD;
        }
    }
}
// returns TRUE (if valid move) or FALSE (if invalid)
int makeMove(TILE grid[10][10], Position playerPositions[MAX_CLIENTS], Move* move, int* numTomato) {
    int player = move->client;
    Position p = playerPositions[player];
    int x = p.x;
    int y = p.y;
    if (x == OFF_BOARD && y == OFF_BOARD) {  // player is not connected anymore, not on board yet
        return FALSE;
    }
    switch (move->dir) {
        case UP:
            y--;
            break;
        case DOWN:
            y++;
            break;
        case LEFT:
            x--;
            break;
        case RIGHT:
            x++;
            break;
    }
    if (x < 0 || y < 0 || x >= GRIDSIZE || y >= GRIDSIZE) {  // check bounds, player cannot go out of bounds
        return FALSE;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {  // if theres already a player there, player cannot move there
        if (playerPositions[i].x == x && playerPositions[i].y == y) {
            return FALSE;
        }
    }
    if (grid[x][y] == TILE_TOMATO) {
        *numTomato = *numTomato - 1;  // update number of tomatoes
        grid[x][y] = TILE_GRASS;
    }
    p.x = x;  // set player x position
    p.y = y;  // set player y postion
    return TRUE;
}

void* eventLoop(void* vargp) {
    Pthread_detach(pthread_self());
    TILE grid[GRIDSIZE][GRIDSIZE];
    Position playerPositions[4];  // keeps track of player positions
    srand(time(NULL));

    for (int i = 0; i < MAX_CLIENTS; i++) {  // set the null player positions
        playerPositions[i].client = i;
        playerPositions[i].x = OFF_BOARD;
        playerPositions[i].y = OFF_BOARD;
    }

    while (1) {  // inifite loop ->
        // init set up the game / each level
        if (numPlayers <= 0) {  // check if there are any players connected
            continue;
        } else {
            printf("num players: %d \n", numPlayers);
        }

        int shouldExit = FALSE;
        initPlayerPositions(playerPositions);
        int numTomatoes = initGrid(grid, playerPositions);

        // write grid & init positions to clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (sockets[i] != EMPTY_SOCKET) {
                Rio_writen(sockets[i], GRID, sizeof(GRID));
                Rio_writen(sockets[i], grid, sizeof(grid));
                Rio_writen(sockets[i], POSITIONS, sizeof(POSITIONS));
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    printf("player %d @ (%d, %d)", playerPositions[i].client, playerPositions[i].x, playerPositions[i].y);
                }
                Rio_writen(sockets[i], playerPositions, sizeof(playerPositions));
                printf("wrote to socket\n");
            }
        }
        pthread_mutex_lock(&lock);
        clearQueue(&eventQueue);  // remove any backlog of events that were not processed from last game
        pthread_mutex_unlock(&lock);
        while (!shouldExit) {
            // game loop
            pthread_mutex_lock(&lock);
            Move* m = dequeue(&eventQueue);
            for (int i = 0; i < MAX_CLIENTS; i++) {  // move all disconnected players off the board
                if (sockets[i] == EMPTY_SOCKET) {
                    playerPositions[i].x = OFF_BOARD;
                    playerPositions[i].y = OFF_BOARD;
                }
            }
            if (numPlayers == 0) {
                shouldExit = TRUE;
            }
            pthread_mutex_unlock(&lock);
            if (m == NULL) {
                continue;
            }
            if (makeMove(grid, playerPositions, m, &numTomatoes)) {  // make the move, if return TRUE then send all updated poisitons to the client
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (sockets[i] != EMPTY_SOCKET) {
                        Rio_writen(sockets[i], playerPositions, sizeof(playerPositions));
                    }
                }
            }
            free(m);  // free the dequeued move
            if (numTomatoes == 0) {
                shouldExit = TRUE;
            }
        }
    }
}

int main(int argc, char** argv) {
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return 1;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        sockets[i] = EMPTY_SOCKET;
    }

    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    Pthread_create(&tid, NULL, eventLoop, NULL);  // create event loop thread

    // int port = atoi(argv[1]);

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        if (numPlayers < MAX_CLIENTS) {  // if the number of clients == 4, then we dont wanna accept more players
            clientlen = sizeof(struct sockaddr_storage);
            connfdp = Malloc(sizeof(int));
            *connfdp = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            printf("some connection has been made\n");
            Pthread_create(&tid, NULL, clientThread, connfdp);
        }
    }
}