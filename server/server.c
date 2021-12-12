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

void listenInputs(int connfd, int player) {
    size_t n;
    Direction dir;
    rio_t rio;
    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readnb(&rio, &dir, sizeof(dir))) != 0) {
        printf("Player %d moved %d ", player, dir);
        switch (dir) {
            case UP:
                printf("UP\n");
                break;
            case DOWN:
                printf("DOWN\n");
                break;
            case LEFT:
                printf("LEFT\n");
                break;
            case RIGHT:
                printf("RIGHT\n");
                break;
        }
        Move* m = (Move*)malloc(sizeof(Move));
        m->dir = dir;
        m->client = player;
        m->next = NULL;
        pthread_mutex_lock(&lock);
        enqueue(&eventQueue, m);
        pthread_mutex_unlock(&lock);
    }
}

// will set up everything for the client, ensuers there are enough spots for the
// cleint, tells client which player they are, etc.
void* clientThread(void* vargp) {
    int connfd = *((int*)vargp);
    Pthread_detach(pthread_self());
    int player = EMPTY_SOCKET;
    Free(vargp);
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_CLIENTS;
         i++) {  // add the fd for the socket into the sockets array, put it in
                 // the first empty (NULL) spot in the array
        if (sockets[i] == EMPTY_SOCKET) {
            sockets[i] = connfd;
            player = i;
            break;
        }
    }
    if (player == -1) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    numPlayers++;
    sockets[player] = connfd;
    Rio_writen(connfd, &player,
               sizeof(player));  // if the player is accepted let them know
                                 // which player they are
    pthread_mutex_unlock(&lock);
    listenInputs(connfd, player);
    Close(connfd);
    pthread_mutex_lock(&lock);
    numPlayers--;
    sockets[player] = EMPTY_SOCKET;
    pthread_mutex_unlock(&lock);
    return NULL;
}

// get a random value in the range [0, 1]
double rand01() { return (double)rand() / (double)RAND_MAX; }

// get a random int from [0,9]
int random10() { return rand() % 10; }

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
    while (numTomatoes == 0) initGrid(grid, playerPositions);
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
                    i--;  // reset this player's random position bc its the same
                          // as another players random position
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

// adds a player to the game mid level, making sure the player is not taking
// occupied space int addPlayer(TILE grid[GRIDSIZE][GRIDSIZE], Position
// playerPositions[MAX_CLIENTS], int player) {
//     int x = random10();
//     int y = random10();
//     playerPositions[player].x = x;
//     playerPositions[player].y = y;
//     for (int i = 0; i < MAX_CLIENTS; i++) {
//         if ((playerPositions[i].x == x && playerPositions[i].y == y) ||
//         grid[x][y] == TILE_TOMATO) {  // check if space is occupied, if it is
//         try again
//             return FALSE;
//         }
//     }
//     return TRUE;
// }
// returns TRUE (if valid move) or FALSE (if invalid)
int makeMove(TILE grid[10][10], Position playerPositions[MAX_CLIENTS],
             Move* move, int* numTomato) {
    int player = move->client;
    Position p = playerPositions[player];
    int x = p.x;
    int y = p.y;
    printf("old x %d, old y %d\n", x, y);
    if (x == OFF_BOARD &&
        y == OFF_BOARD) {  // player is not connected anymore, not on board yet
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
    if (x < 0 || y < 0 || x >= GRIDSIZE ||
        y >= GRIDSIZE) {  // check bounds, player cannot go out of bounds
        return FALSE;
    }
    for (int i = 0; i < MAX_CLIENTS;
         i++) {  // if theres already a player there, player cannot move there
        if (playerPositions[i].x == x && playerPositions[i].y == y) {
            return FALSE;
        }
    }
    if (grid[x][y] == TILE_TOMATO) {
        *numTomato = *numTomato - 1;  // update number of tomatoes
        grid[x][y] = TILE_GRASS;      // upadte grid
    }
    playerPositions[player].x = x;  // set player x position
    playerPositions[player].y = y;  // set player y postion
    return TRUE;
}

// event loop, inits the game for all connected players, and then handles all
// gmae logic, passing on the updared player positions after each move to each
// client
void* eventLoop(void* vargp) {
    Pthread_detach(pthread_self());
    TILE grid[GRIDSIZE][GRIDSIZE];          // game board
    Position playerPositions[MAX_CLIENTS];  // keeps track of player positions
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
                Rio_writen(sockets[i], playerPositions,
                           sizeof(playerPositions));
            }
        }
        pthread_mutex_lock(&lock);
        clearQueue(&eventQueue);  // remove any backlog of events that were not
                                  // processed from last game
        pthread_mutex_unlock(&lock);

        while (!shouldExit) {
            // game loop
            pthread_mutex_lock(&lock);
            Move* m = dequeue(&eventQueue);

            for (int i = 0; i < MAX_CLIENTS;
                 i++) {  // move all disconnected players off the board & add
                         // new players to the board & send board info

                if (sockets[i] == EMPTY_SOCKET) {
                    playerPositions[i].x = OFF_BOARD;
                    playerPositions[i].y = OFF_BOARD;
                }

                // } else if (playerPositions[i].x == OFF_BOARD &&
                // playerPositions[i].y == OFF_BOARD) {  // new player has
                // joined, connfd in sockets[i]

                //     int added;
                //     while ((added = addPlayer(grid, playerPositions, i)) ==
                //     FALSE) {
                //         ;
                //     }
                //     // write the grid
                //     Rio_writen(sockets[i], GRID, sizeof(GRID));
                //     Rio_writen(sockets[i], grid, sizeof(grid));
                //     // write player positions
                //     Rio_writen(sockets[i], POSITIONS, sizeof(POSITIONS));
                //     Rio_writen(sockets[i], playerPositions,
                //     sizeof(playerPositions));
                // }
            }

            if (numPlayers == 0) {
                // if there are no more players, then we want to exit the game,
                // so new players can join
                shouldExit = TRUE;
            }

            pthread_mutex_unlock(&lock);

            if (m == NULL) {  // if the dequeue funciton returns null then we
                              // cant do anything
                continue;
            }

            if (makeMove(
                    grid, playerPositions, m,
                    &numTomatoes)) {  // make the move, if return TRUE then send
                                      // all updated poisitons to the client
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    printf("player %d is @ (%d, %d)\n",
                           playerPositions[i].client, playerPositions[i].x,
                           playerPositions[i].y);
                }
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (sockets[i] != EMPTY_SOCKET) {
                        Rio_writen(sockets[i], POSITIONS, sizeof(POSITIONS));
                        Rio_writen(sockets[i], playerPositions,
                                   sizeof(playerPositions));
                    }
                }
            }

            free(m);  // free the dequeued move

            if (numTomatoes == 0) {
                printf("new game\n");
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
        if (numPlayers <
            MAX_CLIENTS) {  // if the number of clients == MAX_CLIENTS, then we
                            // dont wanna accept more players
            clientlen = sizeof(struct sockaddr_storage);
            connfdp = Malloc(sizeof(int));
            *connfdp = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            printf("some connection has been made\n");
            Pthread_create(&tid, NULL, clientThread, connfdp);
        }
    }
}