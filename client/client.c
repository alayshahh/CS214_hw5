#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "../constants.h"
#include "../csapp.h"

int numTomatoes;

bool shouldExit = false;
rio_t rio;

TTF_Font* font;

// textures
SDL_Renderer* renderer;
SDL_Texture* grassTexture;
SDL_Texture* tomatoTexture;
SDL_Texture* playerTexture;

int clientfd;

void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    int rv = IMG_Init(IMG_INIT_PNG);
    if ((rv & IMG_INIT_PNG) != IMG_INIT_PNG) {
        fprintf(stderr, "Error initializing IMG: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "Error initializing TTF: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }
}

int handleKeyDown(SDL_KeyboardEvent* event) {
    // ignore repeat events if key is held down
    if (event->repeat) return -1;

    if (event->keysym.scancode == SDL_SCANCODE_Q ||
        event->keysym.scancode == SDL_SCANCODE_ESCAPE) {
        shouldExit = true;
        return -1;
    }

    if (event->keysym.scancode == SDL_SCANCODE_UP ||
        event->keysym.scancode == SDL_SCANCODE_W)
        return UP;

    if (event->keysym.scancode == SDL_SCANCODE_DOWN ||
        event->keysym.scancode == SDL_SCANCODE_S)
        return DOWN;

    if (event->keysym.scancode == SDL_SCANCODE_LEFT ||
        event->keysym.scancode == SDL_SCANCODE_A)
        return LEFT;

    if (event->keysym.scancode == SDL_SCANCODE_RIGHT ||
        event->keysym.scancode == SDL_SCANCODE_D)
        return RIGHT;

    return -1;
}

void drawGrid(TILE grid[GRIDSIZE][GRIDSIZE], Position playerPositions[MAX_CLIENTS], int* score) {
    SDL_Rect dest;
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            dest.x = 64 * i;
            dest.y = 64 * j + HEADER_HEIGHT;
            SDL_Texture* texture =
                (grid[i][j] == TILE_GRASS) ? grassTexture : tomatoTexture;
            SDL_QueryTexture(texture, NULL, NULL, &dest.w, &dest.h);
            SDL_RenderCopy(renderer, texture, NULL, &dest);
        }
    }
    // place player textures
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (playerPositions[i].x != OFF_BOARD &&
            playerPositions[i].y != OFF_BOARD) {
            // place player down
            dest.x = 64 * playerPositions[i].x;
            dest.y = 64 * playerPositions[i].y + HEADER_HEIGHT;
            SDL_QueryTexture(playerTexture, NULL, NULL, &dest.w, &dest.h);
            SDL_RenderCopy(renderer, playerTexture, NULL, &dest);
            // is it moving to a tomato?
            int isTomato =
                grid[playerPositions[i].x][playerPositions[i].y] == TILE_TOMATO;
            if (isTomato) {
                grid[playerPositions[i].x][playerPositions[i].y] = TILE_GRASS;
                *score = *score + 1;  // make it collaborative
            }
        }
    }
}

void drawUI(int score, int level) {
    // largest score/level supported is 2147483647
    char scoreStr[18];
    char levelStr[18];
    sprintf(scoreStr, "Score: %d", score);
    sprintf(levelStr, "Level: %d", level);
    SDL_Color white = {255, 255, 255};
    SDL_Surface* scoreSurface = TTF_RenderText_Solid(font, scoreStr, white);
    SDL_Texture* scoreTexture =
        SDL_CreateTextureFromSurface(renderer, scoreSurface);
    SDL_Surface* levelSurface = TTF_RenderText_Solid(font, levelStr, white);
    SDL_Texture* levelTexture =
        SDL_CreateTextureFromSurface(renderer, levelSurface);
    SDL_Rect scoreDest;
    TTF_SizeText(font, scoreStr, &scoreDest.w, &scoreDest.h);
    scoreDest.x = 0;
    scoreDest.y = 0;
    SDL_Rect levelDest;
    TTF_SizeText(font, levelStr, &levelDest.w, &levelDest.h);
    levelDest.x = GRID_DRAW_WIDTH - levelDest.w;
    levelDest.y = 0;
    SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreDest);
    SDL_RenderCopy(renderer, levelTexture, NULL, &levelDest);
    SDL_FreeSurface(scoreSurface);
    SDL_DestroyTexture(scoreTexture);
    SDL_FreeSurface(levelSurface);
    SDL_DestroyTexture(levelTexture);
}

void displayGrid(TILE grid[GRIDSIZE][GRIDSIZE], Position playerPosition[MAX_CLIENTS], int* score, int* level) {
    SDL_SetRenderDrawColor(renderer, 0, 105, 6, 255);
    SDL_RenderClear(renderer);
    drawGrid(grid, playerPosition, score);
    drawUI(*score, *level);
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
}

void* gameListener(void* vargp) {
    Pthread_detach(pthread_self());
    rio_t rio;
    Rio_readinitb(&rio, clientfd);
    int player = -1;
    size_t n;
    n = Rio_readnb(&rio, &player, sizeof(player));
    if (player == -1) {  // not accepted
        printf("server refused connection\n");
        shouldExit = true;
        Close(clientfd);
        // exit(EXIT_FAILURE);
        return NULL;
    }

    TILE grid[GRIDSIZE][GRIDSIZE];
    Position playerPositions[MAX_CLIENTS];

    // make connection to server
    int score = 0;
    int level = 1;
    char input[5];
    printf("size of GRID: %lu, size of POSITIONS %lu, size of input buf %lu\n", sizeof(GRID), sizeof(POSITIONS), sizeof(input));
    bool shouldDisplay = false;
    initSDL();

    font = TTF_OpenFont("../resources/Burbank-Big-Condensed-Bold-Font.otf",
                        HEADER_HEIGHT);
    if (font == NULL) {
        fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_Window* window = SDL_CreateWindow("Client", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                          WINDOW_HEIGHT, 0);
    if (window == NULL) {
        fprintf(stderr, "Error creating app window: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    renderer = SDL_CreateRenderer(window, -1, 0);

    if (renderer == NULL) {
        fprintf(stderr, "Error creating renderer: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    grassTexture = IMG_LoadTexture(renderer, "../resources/grass.png");
    tomatoTexture = IMG_LoadTexture(renderer, "../resources/tomato.png");
    playerTexture = IMG_LoadTexture(renderer, "../resources/player.png");

    while ((n = Rio_readnb(&rio, &input, sizeof(GRID))) != 0) {
        // printf("recieved data from server\n");
        if (input[0] == 'G') {  // next thing we will receive is a grid
            level++;            // come back to this
            Rio_readnb(&rio, &grid, sizeof(grid));
            for (int i = 0; i < MAX_CLIENTS; i++) {  // move all players off the grid so we dont miss a tomato
                playerPositions[i].x = -1;
                playerPositions[i].y = -1;
            }
            shouldDisplay = true;
            displayGrid(grid, playerPositions, &score, &level);
        } else if (input[0] == 'P') {  // we received a 'P' (POSITIONS)
            Rio_readnb(&rio, &playerPositions, sizeof(playerPositions));
            if (shouldDisplay) displayGrid(grid, playerPositions, &score, &level);
        } else if (input[0] == 'L') {
            Rio_readnb(&rio, &level, sizeof(level));
            if (shouldDisplay) displayGrid(grid, playerPositions, &score, &level);
        } else if (input[0] == 'S') {
            Rio_readnb(&rio, &score, sizeof(score));
            if (shouldDisplay) displayGrid(grid, playerPositions, &score, &level);
        } else {
            printf("unknown bytes %s\n", input);
        }
    }
    shouldExit = true;
    Close(clientfd);
    // clean up everything
    SDL_DestroyTexture(grassTexture);
    SDL_DestroyTexture(tomatoTexture);
    SDL_DestroyTexture(playerTexture);

    TTF_CloseFont(font);
    TTF_Quit();

    IMG_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return NULL;
}

int main(int argc, char* argv[]) {
    char *host, *port /*, buf[MAXLINE]*/;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);

    // main game loop
    pthread_t tid;
    Pthread_create(&tid, NULL, gameListener, NULL);  // create event loop thread

    while (!shouldExit) {
        // thread for listening to the game
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            int dir = -1;
            switch (event.type) {
                case SDL_QUIT:
                    shouldExit = true;
                    break;

                case SDL_KEYDOWN:
                    dir = handleKeyDown(&event.key);
                    if (dir == -1) break;
                    // send that direction to the server
                    Rio_writen(clientfd, &dir, sizeof(dir));
                    break;

                default:
                    break;
            }
        }
    }
}
