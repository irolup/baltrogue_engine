/*
 * pthread stub implementation for PS Vita
 * Provides minimal pthread compatibility for libstdc++
 */

#include <pthread.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/error.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

struct vita_pthread_mutex {
    SceUID mutexId;
};

#define VITA_MUTEX_ID(mutex) ((SceUID*)(mutex))

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void)attr;
    if (!mutex) return EINVAL;
    
    char name[32];
    static int mutexCount = 0;
    snprintf(name, 32, "pthread_mutex_%d", mutexCount++);
    
    SceUID mutexId = sceKernelCreateMutex(name, 0x02, 1, NULL);
    if (mutexId < 0) {
        return EAGAIN;
    }
    
    *VITA_MUTEX_ID(mutex) = mutexId;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    SceUID mutexId = *VITA_MUTEX_ID(mutex);
    if (mutexId >= 0) {
        sceKernelDeleteMutex(mutexId);
        *VITA_MUTEX_ID(mutex) = -1;
    }
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    SceUID mutexId = *VITA_MUTEX_ID(mutex);
    if (mutexId < 0) return EINVAL;
    int result = sceKernelLockMutex(mutexId, 1, NULL);
    return (result < 0) ? EAGAIN : 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    SceUID mutexId = *VITA_MUTEX_ID(mutex);
    if (mutexId < 0) return EINVAL;
    int result = sceKernelUnlockMutex(mutexId, 1);
    return (result < 0) ? EAGAIN : 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (!mutex) return EINVAL;
    SceUID mutexId = *VITA_MUTEX_ID(mutex);
    if (mutexId < 0) return EINVAL;
    int result = sceKernelTryLockMutex(mutexId, 1);
    return (result < 0) ? EBUSY : 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                   void *(*start_routine)(void *), void *arg) {
    (void)thread;
    (void)attr;
    (void)start_routine;
    (void)arg;
    return ENOSYS;
}

int pthread_join(pthread_t thread, void **retval) {
    (void)thread;
    (void)retval;
    return ENOSYS;
}

pthread_t pthread_self(void) {
    return (pthread_t)sceKernelGetThreadId();
}

int pthread_equal(pthread_t t1, pthread_t t2) {
    return (t1 == t2) ? 1 : 0;
}

void pthread_exit(void *retval) {
    (void)retval;
    sceKernelExitDeleteThread(0);
}

typedef struct {
    SceUID semId;
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    (void)pshared;
    if (!sem) return -1;
    
    char name[32];
    static int semCount = 0;
    snprintf(name, 32, "sem_%d", semCount++);
    
    SceUID semId = sceKernelCreateSema(name, 0, value, 1, NULL);
    if (semId < 0) return -1;
    
    *((SceUID*)sem) = semId;
    return 0;
}

int sem_destroy(sem_t *sem) {
    if (!sem) return -1;
    SceUID semId = *((SceUID*)sem);
    if (semId >= 0) {
        sceKernelDeleteSema(semId);
        *((SceUID*)sem) = -1;
    }
    return 0;
}

int sem_post(sem_t *sem) {
    if (!sem) return -1;
    SceUID semId = *((SceUID*)sem);
    if (semId < 0) return -1;
    int result = sceKernelSignalSema(semId, 1);
    return (result < 0) ? -1 : 0;
}

int sem_wait(sem_t *sem) {
    if (!sem) return -1;
    SceUID semId = *((SceUID*)sem);
    if (semId < 0) return -1;
    int result = sceKernelWaitSema(semId, 1, NULL);
    return (result < 0) ? -1 : 0;
}

int sem_trywait(sem_t *sem) {
    if (!sem || sem->semId < 0) return -1;
    int result = sceKernelPollSema(sem->semId, 1);
    return (result < 0) ? -1 : 0;
}

