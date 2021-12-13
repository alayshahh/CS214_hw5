#include "queue.h"

#include <stdlib.h>

void enqueue(MoveQueue* queue, Move* move) {
    if (queue->head == NULL) {
        queue->tail = move;
        queue->head = move;
        move->next = NULL;
    } else {
        queue->tail->next = move;
        move->next = NULL;
    }
}
Move* dequeue(MoveQueue* queue) {
    Move* move = queue->head;
    if (move != NULL) {
        queue->head = move->next;
    }
    return move;
}
void clearQueue(MoveQueue* queue) {
    Move* m;
    while ((m = dequeue(queue)) != NULL) {
        free(m);
    }
}