#ifndef THREADS_H
#ifdef USE_PTHREADS
#include <pthread.h>
#endif
/*
 *   Basic threading primitives using pthread.  Right now we statically
 *   limit the number of threads, but there's really no good reason to
 *   do this but laziness.
 */
void init_mutex() ;
void get_global_lock() ;
void release_global_lock() ;
#define THREAD_RETURN_TYPE void *
#define THREAD_DECLARATOR
#ifdef USE_PTHREADS
void spawn_thread(int, THREAD_RETURN_TYPE(THREAD_DECLARATOR *p)(void *), void *) ;
void join_thread(int i) ;
#endif
#ifdef USE_PTHREADS
const int MAXTHREADS = 64 ;
#else
const int MAXTHREADS = 1 ;
#endif
/*
 *   This sets a limit on the scalability of filling, but at the same
 *   time introduces a need for more memory since we need
 *   MAXTHREADS * MEMSHARDS * FILLCHUNKS * sizeof(ull) for shard
 *   buffers.
 */
const int MEMSHARDS = 64 ;
struct memshard {
#ifdef USE_PTHREADS
   pthread_mutex_t mutex ;
#endif
   char pad[256] ;
} ;
extern memshard memshards[MEMSHARDS] ;
void init_threads() ;
#ifdef USE_PTHREADS
extern pthread_mutex_t mmutex ;
extern pthread_t p_thread[MAXTHREADS] ;
#endif
extern int numthreads ;
#define THREADS_H
#endif
