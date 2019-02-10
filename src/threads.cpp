#include "threads.h"
#include <pthread.h>

int numthreads = 4 ;
pthread_mutex_t mmutex ;
pthread_t p_thread[MAXTHREADS] ;
memshard memshards[MEMSHARDS] ;
void init_mutex() {
  pthread_mutex_init(&mmutex, NULL) ;
}
void get_global_lock() {
   pthread_mutex_lock(&mmutex) ;
}
void release_global_lock() {
   pthread_mutex_unlock(&mmutex) ;
}
void spawn_thread(int i, THREAD_RETURN_TYPE(THREAD_DECLARATOR *p)(void *),
                                                                    void *o) {
   pthread_create(&(p_thread[i]), NULL, p, o) ;
}
void join_thread(int i) {
   pthread_join(p_thread[i], 0) ;
}
void init_threads() {
   init_mutex() ;
   for (int i=0; i<MEMSHARDS; i++)
      pthread_mutex_init(&(memshards[i].mutex), NULL) ;
}
