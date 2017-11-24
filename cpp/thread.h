//
// Created by root on 17. 11. 20.
//

#ifndef KOREAPASSING_THREAD_H
#define KOREAPASSING_THREAD_H

#include <pthread.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/*#if (!defined(HAVE_STRUCT_TIMESPEC) && !defined(_TIMESPEC_DEFINED))
#define HAVE_STRUCT_TIMESPEC 1
#define _TIMESPEC_DEFINED 1
struct timespec {
    long tv_sec;
    long tv_nsec;
};
#endif *//* HAVE_STRUCT_TIMESPEC | _TIMESPEC_DEFINED */

#define usbi_mutex_static_t        pthread_mutex_t
#define USBI_MUTEX_INITIALIZER        PTHREAD_MUTEX_INITIALIZER
#define usbi_mutex_static_lock        pthread_mutex_lock
#define usbi_mutex_static_unlock    pthread_mutex_unlock

#define usbi_mutex_t            pthread_mutex_t
#define usbi_mutex_init        pthread_mutex_init
#define usbi_mutex_lock            pthread_mutex_lock
#define usbi_mutex_unlock        pthread_mutex_unlock
#define usbi_mutex_trylock        pthread_mutex_trylock
#define usbi_mutex_destroy        pthread_mutex_destroy

#define usbi_cond_t            pthread_cond_t
#define usbi_cond_init        pthread_cond_init
#define usbi_cond_wait            pthread_cond_wait
#define usbi_cond_broadcast        pthread_cond_broadcast
#define usbi_cond_destroy        pthread_cond_destroy

#define usbi_tls_key_t            pthread_key_t
#define usbi_tls_key_create    pthread_key_create
#define usbi_tls_key_get        pthread_getspecific
#define usbi_tls_key_set        pthread_setspecific
#define usbi_tls_key_delete        pthread_key_delete

int usbi_cond_timedwait(pthread_cond_t *cond,
                        pthread_mutex_t *mutex, const struct timeval *tv);

int usbi_get_tid(void);


#endif //KOREAPASSING_THREAD_H
