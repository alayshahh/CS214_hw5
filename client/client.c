#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "../constants.h"
#include "../csapp.h"

TILE grid[GRIDSIZE][GRIDSIZE];

Position playerPositions[MAX_CLIENTS];

int score;
int level;
int numTomatoes;
int playerNum;

bool shouldExit = false;
rio_t rio;

TTF_Font* font;

// textures
SDL_Renderer* renderer;
SDL_Texture* grassTexture;
SDL_Texture* tomatoTexture;
SDL_Texture* playerTexture;

int clientfd;

// get a random value in the range [0, 1]
double rand01() { return (double)rand() / (double)RAND_MAX; }

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

Direction handleKeyDown(SDL_KeyboardEvent* event) {
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

void* processInputs(void* vargp) {
    Pthread_detach(pthread_self());
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        Direction dir;
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
    return NULL;
}

void drawGrid() {
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

            // increment score if i is playerNum
            if (isTomato) {
                grid[playerPositions[i].x][playerPositions[i].y] = TILE_GRASS;
                if (i == playerNum) score++;
            }
        }
    }
}

void drawUI() {
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

void displayGrid() {
    SDL_SetRenderDrawColor(renderer, 0, 105, 6, 255);
    SDL_RenderClear(renderer);

    drawGrid();
    drawUI();

    SDL_RenderPresent(renderer);

    SDL_Delay(16);
}

void* gameListener(void* vargp) {
    Pthread_detach(pthread_self());
    size_t n;
    char input[2];

    while ((n = Rio_readnb(&rio, &input, 2)) != 0) {
        if (input[0] == 'G') {  // next thing we will receive is a grid
            level++;            // come back to this
            printf("receved grid object \n");
            Rio_readnb(&rio, &grid, sizeof(grid));
            displayGrid();
        } else {  // we received a 'P' (POSITIONS)
            Rio_readnb(&rio, &playerPositions, sizeof(playerPositions));
            displayGrid();
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    srand(time(NULL));

    level = 1;

    initSDL();

    font = TTF_OpenFont("resources/Burbank-Big-Condensed-Bold-Font.otf",
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

    grassTexture = IMG_LoadTexture(renderer, "resources/grass.png");
    tomatoTexture = IMG_LoadTexture(renderer, "resources/tomato.png");
    playerTexture = IMG_LoadTexture(renderer, "resources/player.png");

    // make connection to server
    char *host, *port /*, buf[MAXLINE]*/;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    // read player num
    size_t n = Rio_readnb(&rio, &playerNum, sizeof(playerNum));
    if (n == 0) shouldExit = true;

    // main game loop
    while (!shouldExit) {
        // thread for listening to the game
        pthread_t tid;
        Pthread_create(&tid, NULL, gameListener,
                       NULL);  // create event loop thread

        // thread for listening to the player's moves
        pthread_t tid2;
        Pthread_create(&tid2, NULL, processInputs, NULL);
    }

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
}
