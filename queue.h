#include "constants.h"

typedef struct MoveQueue{
    Move* head;
    Move* tail;
} MoveQueue;

void enqueue(MoveQueue* queue, Move* move);
Move* dequeue(MoveQueue* queue);
void clearQueue(MoveQueue* queue);