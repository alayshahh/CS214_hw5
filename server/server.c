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

// void echo(int connfd) {
//     size_t n;
//     char buf[MAXLINE];
//     rio_t rio;
//     Rio_readinitb(&rio, connfd);
//     while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
//         printf("server received %d bytes\n", (int)n);
//         Rio_writen(connfd, buf, n);
//     }
// }

// void *thread(void *vargp) {
//     int connfd = *((int *)vargp);
//     Pthread_detach(pthread_self());
//     Free(vargp);
//     pthread_mutex_lock(&lock);
//     numPlayers++;
//     pthread_mutex_unlock(&lock);
//     echo(connfd);
//     Close(connfd);
//     pthread_mutex_lock(&lock);
//     numPlayers++;
//     pthread_mutex_unlock(&lock);
//     return NULL;
// }

// get a random value in the range [0, 1]
double rand01() {
    return (double)rand() / (double)RAND_MAX;
}

// get a random int from [0,9]
int random10() {
    return rand() % 10;
}

int initGrid(TILE** grid, Position* playerPositions) {
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
        if (x == -1 && y == -1) {
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

void initPlayerPositions(Position* playerPostions) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sockets[i] != NULL) {
            int x = random10();
            int y = random10();
            for (int j = 0; j < i; j++) {
                if (x == playerPostions[i].x && y == playerPostions[i].y) {
                    i--;  // reset this player's random position bc its the same as another players random position
                    break;
                }
            }
        } else {  // if no connection to that player, set the position to -1,-1
            playerPostions[i].x = -1;
            playerPostions[i].y = -1;
        }
    }
}
// returns TRUE (if valid move) or FALSE (if invalid)
int makeMove(TILE** grid, Position* playerPositions, Move* move, int* numTomato) {
    int player = move->client;
    Position p = playerPositions[player];
    int x = p.x;
    int y = p.y;
    if (x == -1 && y == -1) {  // player is not connected anymore, not on board yet
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
    p.x = x;   // set player x position
    p, y = y;  // set player y postion
    return TRUE;
}

void eventLoop(void* vargp) {
    TILE grid[GRIDSIZE][GRIDSIZE];
    Position playerPositions[4];  // keeps track of player positions
    srand(time(NULL));

    for (int i = 0; i < 4; i++) {  // set the null player positions
        playerPositions->client = i;
        playerPositions->x = -1;
        playerPositions->y = -1;
    }
    while (1) {
        // init set up the game / each level
        if (numPlayers <= 0) {  // check if there are any players connected
            continue;
        }

        int shouldExit = FALSE;
        initPlayerPositions(playerPositions);
        int numTomatoes = initGrid(grid, playerPositions);

        // write grid & init positions to clients
        for (int i = 0; i < MAX_CLIENTS; i) {
            if (sockets[i] != NULL) {
                Rio_writen(sockets[i], grid, sizeof(grid));
                Rio_writen(sockets[i], playerPositions, sizeof(playerPositions));
            }
        }
        pthread_mutex_lock(&lock);
        clearQueue(&eventQueue);
        pthread_mutex_unlock(&lock);
        while (!shouldExit) {
            // game loop
            pthread_mutex_lock(&lock);
            Move* m = dequeue(&eventQueue);
            for (int i = 0; i < MAX_CLIENTS) {  // move all disconnected players off the board
                if (sockets[i] == NULL) {
                    playerPositions[i].x = -1;
                    playerPositions[i].y = -1;
                }
            }
            if (numPlayers == 0) {
                shouldExit = TRUE;
            }
            pthread_mutex_unlock(&lock);
            if (m == NULL) {
                continue;
            }
            if (makeMove(grid, playerPositions, m, &numTomatoes)) {  // make the move, if return true then send all updated poisitons to the client
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (sockets[i] != NULL) {
                        Rio_writen(sockets[i], playerPositions, sizeof(playerPositions));
                    }
                }
            }
            free(m);  // free the dqueued move
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

    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    // Pthread_create(&tid, NULL,   ) // create event loop thread

    int port = atoi(argv[1]);

    listenfd = Open_listenfd(port);
    while (1) {
        if (numPlayers < MAX_CLIENTS) {  // if the number of clients == 4, then we dont wanna accept more players
            clientlen = sizeof(struct sockaddr_storage);
            connfdp = Malloc(sizeof(int));
            *connfdp = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            Pthread_create(&tid, NULL, thread, connfdp);
        }
    }
}