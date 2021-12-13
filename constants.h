#ifndef CONSTANTS_H
#define CONSTANTS_H
typedef struct {
    int x;
    int y;
    int client;  // 1,2,... MAX_CLIENT
} Position;

typedef enum {
    TILE_GRASS,
    TILE_TOMATO
    // TILE_PLAYER
} TILE;

typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

typedef struct Move {
    Direction dir;
    int client;  // 1,2,...MAX_CLIENT
    struct Move* next;
} Move;

// Dimensions for the drawn grid (should be GRIDSIZE * texture dimensions)

#define GRID_DRAW_WIDTH 640

#define GRID_DRAW_HEIGHT 640

#define WINDOW_WIDTH GRID_DRAW_WIDTH

#define WINDOW_HEIGHT (HEADER_HEIGHT + GRID_DRAW_HEIGHT)

// Header displays current score

#define HEADER_HEIGHT 50

// Number of cells vertically/horizontally in the grid

#define GRIDSIZE 10

#define TRUE 1

#define FALSE 0

#define MAX_CLIENTS 4

#define EMPTY_SOCKET -1

#define OFF_BOARD -1

#define GRID "G"

#define POSITIONS "P"

#define LEVEL "L"

#define SCORE "S"

#endif