/*****************************************************************************
 * life.c
 * The original sequential implementation resides here.
 * Do not modify this file, but you are encouraged to borrow from it
 ****************************************************************************/
#include "life.h"
#include "util.h"
#include <pthread.h>

/**
 * Swapping the two boards only involves swapping pointers, not
 * copying values.
 */
#define SWAP_BOARDS( b1, b2 )  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

#define BOARD( __board, __i, __j )  (__board[LDA*(__i) + (__j)])

#define NUM_THREADS 4

typedef struct thread_args {
    char* outboard;
    char* inboard;
    int nrows;
    int ncols;
    int rStart;
    int rEnd;
    int cStart;
    int cEnd;
} t_args_t;

void *thread(void *arg) {
    int i, j;
    char* outboard = ((t_args_t *)arg)->outboard;
    char* inboard = ((t_args_t *)arg)->inboard;
    const int nrows = ((t_args_t *)arg)->nrows;
    const int ncols = ((t_args_t *)arg)->ncols;
    const int rStart = ((t_args_t *)arg)->rStart;
    const int rEnd = ((t_args_t *)arg)->rEnd;
    const int cStart = ((t_args_t *)arg)->cStart;
    const int cEnd = ((t_args_t *)arg)->cEnd;

    const int LDA = nrows;

    for (i = rStart; i < rEnd; i++) {
        for (j = cStart; j < cEnd; j++) {
            const int inorth = mod (i-1, nrows);
            const int isouth = mod (i+1, nrows);
            const int jwest = mod (j-1, ncols);
            const int jeast = mod (j+1, ncols);

            const char neighbor_count = 
                BOARD (inboard, inorth, jwest) + 
                BOARD (inboard, inorth, j) + 
                BOARD (inboard, inorth, jeast) + 
                BOARD (inboard, i, jwest) +
                BOARD (inboard, i, jeast) + 
                BOARD (inboard, isouth, jwest) +
                BOARD (inboard, isouth, j) + 
                BOARD (inboard, isouth, jeast);
            BOARD(outboard, i, j) = alivep (neighbor_count, BOARD (inboard, i, j));
        }
    }
    pthread_exit(NULL);
}


char*
sequential_game_of_life (char* outboard, 
        char* inboard,
        const int nrows,
        const int ncols,
        const int gens_max)
{
    /* HINT: in the parallel decomposition, LDA may not be equal to
       nrows! */
    // parallelization happens here
    pthread_t tid[NUM_THREADS];
    t_args_t args[NUM_THREADS*2];    
    int curgen, i;
    // set up static args for threads 8x1
    /*(for (i = 0; i < NUM_THREADS; i++) {
        args[i].nrows = nrows;
        args[i].ncols = ncols;
        args[i].rStart = (nrows/(NUM_THREADS*2)) * (i);
        args[i].rEnd = (nrows/(NUM_THREADS*2)) * (i+1);
        args[i].cStart = 0;
        args[i].cEnd = ncols;
    }
    for (i = 0; i < NUM_THREADS; i++) {
        args[NUM_THREADS+i].nrows = nrows;
        args[NUM_THREADS+i].ncols = ncols;
        args[NUM_THREADS+i].rStart = (nrows/(NUM_THREADS*2)) * (NUM_THREADS+i);
        args[NUM_THREADS+i].rEnd = (nrows/(NUM_THREADS*2)) * (NUM_THREADS+i+1);
        args[NUM_THREADS+i].cStart = 0;
        args[NUM_THREADS+i].cEnd = ncols;
    }*/
    // 4x2
    for (i = 0; i < NUM_THREADS; i++) {
        args[i].nrows = nrows;
        args[i].ncols = ncols;
        args[i].rStart = (nrows/(NUM_THREADS)) * (i%2);
        args[i].rEnd = (nrows/(NUM_THREADS)) * ((i%2)+1);
        args[i].cStart = (ncols/2) * (i/2);
        args[i].cEnd = (ncols/2) * ((i/2)+1);
    }
    for (i = 0; i < NUM_THREADS; i++) {
        args[NUM_THREADS+i].nrows = nrows;
        args[NUM_THREADS+i].ncols = ncols;
        args[NUM_THREADS+i].rStart = (nrows/(NUM_THREADS)) * (NUM_THREADS/2+(i%2));
        args[NUM_THREADS+i].rEnd = (nrows/(NUM_THREADS)) * (NUM_THREADS/2+(i%2)+1);
        args[NUM_THREADS+i].cStart = (ncols/2) * (i/2);
        args[NUM_THREADS+i].cEnd = (ncols/2) * ((i/2)+1);
    }

    for (curgen = 0; curgen < gens_max; curgen++)
    {
        /* HINT: you'll be parallelizing these loop(s) by doing a
           geometric decomposition of the output */
        for (i = 0; i < NUM_THREADS; i++) {
            args[i].outboard = outboard;
            args[i].inboard = inboard;
            pthread_create(&tid[i], NULL, thread, &args[i]);
        }
        for (i = 0; i < NUM_THREADS; i++) {
            pthread_join(tid[i], NULL);
        }
        for (i = 0; i < NUM_THREADS; i++) {
            args[NUM_THREADS+i].outboard = outboard;
            args[NUM_THREADS+i].inboard = inboard;
            pthread_create(&tid[i], NULL, thread, &args[NUM_THREADS+i]);
        }
        for (i = 0; i < NUM_THREADS; i++) {
            pthread_join(tid[i], NULL);
        }
        SWAP_BOARDS( outboard, inboard );
    }
    /* 
     * We return the output board, so that we know which one contains
     * the final result (because we've been swapping boards around).
     * Just be careful when you free() the two boards, so that you don't
     * free the same one twice!!! 
     */
    return inboard;
}
