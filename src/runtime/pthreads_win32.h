#ifndef WIN32_PTHREAD_INCLUDED
#define WIN32_PTHREAD_INCLUDED
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* 0 - Misc */
void pthreads_win32_init();

/* 1 - Thread */

typedef HANDLE pthread_t;

typedef struct pthread_attr_t {
  unsigned int stack_size;
} pthread_attr_t;

int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);

typedef void (*pthread_cleanup_fn)(void* arg);

#define pthread_cleanup_push(fn, arg) { pthread_cleanup_fn __pthread_fn = fn; void *__pthread_arg = arg;
#define pthread_cleanup_pop(execute) if (execute) __pthread_fn(__pthread_arg); }

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
int pthread_equal(pthread_t thread1, pthread_t thread2);
int pthread_detach(pthread_t thread);
int pthread_join(pthread_t thread, void **retval);
pthread_t pthread_self(void);

typedef DWORD pthread_key_t;
int pthread_key_create(pthread_key_t *key, void (*destructor)(void*));
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);

typedef int sigset_t;
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);

/* 1a - Thread non-portable */

void pthread_np_safepoint();
void pthread_np_suspend(pthread_t thread);
void pthread_np_resume(pthread_t thread);
unsigned char pthread_np_interruptible(pthread_t thread);
void pthread_np_request_interruption(pthread_t thread);
void pthread_np_enter_uninterruptible();
void pthread_np_leave_uninterruptible();

/* 2 - Mutex */

typedef CRITICAL_SECTION* pthread_mutex_t;
typedef int pthread_mutexattr_t;
#define PTHREAD_MUTEX_INITIALIZER ((pthread_mutex_t)-1)
int pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t * attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* 3 - Condition variable */

typedef struct thread_wakeup {
  HANDLE event;
  struct thread_wakeup *next;
} thread_wakeup;

typedef HANDLE (*cv_event_get_fn)();
typedef void (*cv_event_return_fn)(HANDLE event);

typedef struct pthread_cond_t {
  pthread_mutex_t wakeup_lock;
  struct thread_wakeup *first_wakeup;
  struct thread_wakeup *last_wakeup;
  unsigned char alertable;
  cv_event_get_fn get_fn;
  cv_event_return_fn return_fn;
} pthread_cond_t;

typedef struct pthread_condattr_t {
  unsigned char alertable;
  cv_event_get_fn get_fn;
  cv_event_return_fn return_fn;
} pthread_condattr_t;

typedef struct timespec {
  time_t tv_sec;
  long tv_nsec;
} timespec;

// not implemented: PTHREAD_COND_INITIALIZER
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t * attr);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_timedwait(pthread_cond_t * cond, pthread_mutex_t * mutex, const struct timespec * abstime);
int pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex);

#define ETIMEDOUT 123 //Something

int sched_yield();

#endif
