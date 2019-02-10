#ifndef THREADS_H
#include <pthread.h>
void init_mutex() ;
void get_global_lock() ;
void release_global_lock() ;
#define THREAD_RETURN_TYPE void *
#define THREAD_DECLARATOR
void spawn_thread(int, THREAD_RETURN_TYPE(THREAD_DECLARATOR *p)(void *), void *) ;
void join_thread(int i) ;
const int MAXTHREADS = 64 ;
/*
 *   This sets a limit on the scalability of filling, but at the same
 *   time introduces a need for more memory since we need
 *   MAXTHREADS * MEMSHARDS * FILLCHUNKS * sizeof(ull) for shard
 *   buffers.
 */
const int MEMSHARDS = 64 ;
struct memshard {
   pthread_mutex_t mutex ;
   char pad[256] ;
} ;
extern memshard memshards[MEMSHARDS] ;
void init_threads() ;
extern pthread_mutex_t mmutex ;
extern int numthreads ;
extern pthread_t p_thread[MAXTHREADS] ;
#define THREADS_H
#endif
