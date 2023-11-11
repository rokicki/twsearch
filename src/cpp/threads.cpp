#include "threads.h"
#ifdef USE_PTHREADS
#include <pthread.h>
#endif

int numthreads;
#ifdef USE_PTHREADS
pthread_mutex_t mmutex;
pthread_t p_thread[MAXTHREADS];
#endif
memshard memshards[MEMSHARDS];
void init_mutex() {
#ifdef USE_PTHREADS
  pthread_mutex_init(&mmutex, NULL);
#endif
}
void get_global_lock() {
#ifdef USE_PTHREADS
  pthread_mutex_lock(&mmutex);
#endif
}
void release_global_lock() {
#ifdef USE_PTHREADS
  pthread_mutex_unlock(&mmutex);
#endif
}
#ifdef USE_PTHREADS
void spawn_thread(int i, THREAD_RETURN_TYPE(THREAD_DECLARATOR *p)(void *),
                  void *o) {
  pthread_create(&(p_thread[i]), NULL, p, o);
}
void join_thread(int i) { pthread_join(p_thread[i], 0); }
#endif
void init_threads() {
#ifdef USE_PTHREADS
  init_mutex();
  for (int i = 0; i < MEMSHARDS; i++)
    pthread_mutex_init(&(memshards[i].mutex), NULL);
#endif
}
