#include "pthreads_win32.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

int pthread_attr_init(pthread_attr_t *attr)
{
  attr->stack_size = 0;
  return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
  return 0;
}

int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize)
{
  fprintf(stderr, "pthread_attr_setstack called\n");
  ExitProcess(1);
  return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
  attr->stack_size = stacksize;
  return 0;
}

typedef void *(*pthread_fn)(void*);

DWORD thread_self_tls_index;

typedef struct thread_args {
  pthread_fn start_routine;
  void* arg;
  HANDLE self;
} thread_args;

DWORD WINAPI Thread_Function(LPVOID param)
{
  thread_args *args = (thread_args*)param;
  void* arg = args->arg;
  pthread_fn fn = args->start_routine;
  TlsSetValue(thread_self_tls_index, args->self);
  free(args);
  return (DWORD)fn(arg);
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
  thread_args* args = (thread_args*)malloc(sizeof(thread_args));
  args->start_routine = start_routine;
  args->arg = arg;
  HANDLE createdThread = CreateThread(NULL, attr ? attr->stack_size : 0, Thread_Function, args, CREATE_SUSPENDED, NULL);
  if (!createdThread)
    return 1;
  args->self = createdThread;
  ResumeThread(createdThread);
  if (thread)
    *thread = createdThread;
  return 0;
}

int pthread_detach(pthread_t thread)
{
  // FIXME: What to do??
  return 0;
}

int pthread_join(pthread_t thread, void **retval)
{
  if (WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0) {
    CloseHandle(thread);
    return 0;
  } else {
    CloseHandle(thread);
    return 1;
  }
}

pthread_t pthread_self(void)
{
  return (HANDLE)TlsGetValue(thread_self_tls_index);
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
{
  DWORD index;
  if (destructor) {
    fprintf(stderr, "destructor is specified for pthread_key_create\n");
    ExitProcess(1);
  }
  index = TlsAlloc();
  if (index == TLS_OUT_OF_INDEXES)
    return 1;
  *key = index;
  return 0;
}

void *pthread_getspecific(pthread_key_t key)
{
  return TlsGetValue(key);
}

int pthread_setspecific(pthread_key_t key, const void *value)
{
  return TlsSetValue(key, (LPVOID)value) != FALSE;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset)
{
  fprintf(stderr, "pthread_sigmask called\n");
  ExitProcess(1);
  return 1;
}

CRITICAL_SECTION mutex_init_lock;

int pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t * attr)
{
  *mutex = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
  InitializeCriticalSection(*mutex);
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  if (*mutex != PTHREAD_MUTEX_INITIALIZER)
    DeleteCriticalSection(*mutex);
  return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
  if (*mutex == PTHREAD_MUTEX_INITIALIZER) {
    EnterCriticalSection(&mutex_init_lock);
    if (*mutex == PTHREAD_MUTEX_INITIALIZER) {
      *mutex = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
      pthread_mutex_init(mutex, NULL);
    }
    LeaveCriticalSection(&mutex_init_lock);
  }
  EnterCriticalSection(*mutex);
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  LeaveCriticalSection(*mutex);
  return 0;
}

static HANDLE cv_default_event_get_fn()
{
  return CreateEvent(NULL, FALSE, FALSE, NULL);
}

static void cv_default_event_return_fn(HANDLE event)
{
  CloseHandle(event);
}

int pthread_cond_init(pthread_cond_t * cv, const pthread_condattr_t * attr)
{
  InitializeCriticalSection(&cv->wakeup_lock);
  cv->first_wakeup = NULL;
  cv->last_wakeup = NULL;
  cv->alertable = 0;
  cv->get_fn = cv_default_event_get_fn;
  cv->return_fn = cv_default_event_return_fn;
  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cv)
{
  DeleteCriticalSection(&cv->wakeup_lock);
  return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cv)
{
  int count = 0;
  EnterCriticalSection(&cv->wakeup_lock);
  while (cv->first_wakeup)
  {
    struct thread_wakeup * w = cv->first_wakeup;
    cv->first_wakeup = w->next;
    SetEvent(w->event);
    ++count;
  }
  cv->last_wakeup = NULL;
  LeaveCriticalSection(&cv->wakeup_lock);
  return 0;
}

int pthread_cond_signal(pthread_cond_t *cv)
{
  struct thread_wakeup * w;
  EnterCriticalSection(&cv->wakeup_lock);
  w = cv->first_wakeup;
  if (!w) return 0;
  cv->first_wakeup = w->next;
  if (!cv->first_wakeup) cv->last_wakeup = NULL;
  SetEvent(w->event);
  LeaveCriticalSection(&cv->wakeup_lock);
  return 0;
}

void cv_wakeup_add(struct pthread_cond_t* cv, struct thread_wakeup* w)
{
  w->event = cv->get_fn();
  w->next = NULL;
  EnterCriticalSection(&cv->wakeup_lock);
  if (cv->last_wakeup != NULL)
  {
    cv->last_wakeup->next = w;
    cv->last_wakeup = w;
  }
  else
  {
    cv->first_wakeup = w;
    cv->last_wakeup = w;
  }
  //fprintf(stderr, "added wakeup:\n");
  //cv_print_wakeups(cv);
  LeaveCriticalSection(&cv->wakeup_lock);
}

int pthread_cond_wait(pthread_cond_t * cv, pthread_mutex_t * cs)
{
  struct thread_wakeup w;
  cv_wakeup_add(cv, &w);
  LeaveCriticalSection(*cs);
  if (cv->alertable) {
    while (WaitForSingleObjectEx(w.event, INFINITE, TRUE) == WAIT_IO_COMPLETION);
  } else {
    WaitForSingleObject(w.event, INFINITE);
  }
  cv->return_fn(w.event);
  EnterCriticalSection(*cs);
  return 0;
}

int pthread_cond_timedwait(pthread_cond_t * cv, pthread_mutex_t * cs, const struct timespec * abstime)
{
  DWORD rv;
  struct thread_wakeup w;
  cv_wakeup_add(cv, &w);
  LeaveCriticalSection(*cs);
  {
    struct timeval cur_tm;
    long sec, msec;
    gettimeofday(&cur_tm, NULL);
    sec = abstime->tv_sec - cur_tm.tv_sec;
    msec = sec * 1000 + abstime->tv_nsec / 1000000 - cur_tm.tv_usec / 1000;
    //fprintf(stderr, "Waiting for %ld msec\n", msec);
    if (msec < 0)
      msec = 0;
    if (cv->alertable) {
      while ((rv = WaitForSingleObjectEx(w.event, msec, TRUE)) == WAIT_IO_COMPLETION);
    } else {
      rv = WaitForSingleObject(w.event, msec);
    }
    //fprintf(stderr, "Waiting done (%s)\n", rv == WAIT_TIMEOUT ? "timeout" : "condition_signalled");
  }
  cv->return_fn(w.event);
  //fprintf(stderr, "Condvar, reentering CS\n");
  EnterCriticalSection(*cs);
  //fprintf(stderr, "Condvar, in CS\n");
  if (rv == WAIT_TIMEOUT)
    return ETIMEDOUT;
  else
    return 0;
}

int sched_yield()
{
  SwitchToThread();
  return 0;
}

void pthreads_win32_init()
{
  HANDLE self_handle;
  DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &self_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
  thread_self_tls_index = TlsAlloc();
  TlsSetValue(thread_self_tls_index, self_handle);
  InitializeCriticalSection(&mutex_init_lock);
}
