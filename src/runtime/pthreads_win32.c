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

typedef unsigned char boolean;

typedef struct thread_args {
  pthread_fn start_routine;
  void* arg;
  HANDLE handle;
  pthread_cond_t *waiting_cond;
  int uninterruptible_section_nesting;
  unsigned int in_safepoint;
  struct thread_args *prev, *next;
} thread_args;

thread_args * pthread_all_threads;
pthread_mutex_t pthread_all_threads_lock;

void pthread_link_thread_args(thread_args *args)
{
  pthread_mutex_lock(&pthread_all_threads_lock);
  if (pthread_all_threads) {
    args->next = pthread_all_threads;
    args->prev = pthread_all_threads->prev;
    args->next->prev = args;
    args->prev->next = args;
    pthread_all_threads = args;
  } else {
    args->next = args;
    args->prev = args;
    pthread_all_threads = args;
  }
  pthread_mutex_unlock(&pthread_all_threads_lock);
}

void pthread_unlink_thread_args(thread_args *args)
{
  pthread_mutex_lock(&pthread_all_threads_lock);
  if (args->next == args) {
    pthread_all_threads = NULL;
  } else {
    args->next->prev = args->prev;
    args->prev->next = args->next;
    pthread_all_threads = args->prev;
  }
  pthread_mutex_unlock(&pthread_all_threads_lock);
}

thread_args* pthread_find_thread_args(HANDLE handle)
{
  thread_args* found = NULL, *i;
  pthread_mutex_lock(&pthread_all_threads_lock);
  if (pthread_all_threads == NULL) {
    found = NULL;
  } else if (pthread_all_threads->handle == handle) {
    found = pthread_all_threads;
  } else {
    for (i = pthread_all_threads->next; i != pthread_all_threads; i = i->next) {
      if (i->handle == handle) {
        found = i;
        break;
      }
    }
  }
  pthread_mutex_unlock(&pthread_all_threads_lock);
  return found;
}

void pthread_np_suspend(pthread_t thread)
{
  CONTEXT context;
  SuspendThread(thread);
  context.ContextFlags = CONTEXT_FULL;
  GetThreadContext(thread, &context);
}

void pthread_np_resume(pthread_t thread)
{
  ResumeThread(thread);
}

unsigned char pthread_np_interruptible(pthread_t thread)
{
  thread_args* args = pthread_find_thread_args(thread);
  if (args == NULL)
    return 0;
  return args->uninterruptible_section_nesting == 0 && args->waiting_cond == NULL;
}

void pthread_np_request_interruption(pthread_t thread)
{
  thread_args* args;
  args = pthread_find_thread_args(thread);
  if (args == NULL)
    return;
  if (args->waiting_cond) {
    pthread_cond_broadcast(args->waiting_cond);
  }
}

thread_args* pthread_self_args()
{
  return (thread_args*)TlsGetValue(thread_self_tls_index);
}

DWORD WINAPI Thread_Function(LPVOID param)
{
  thread_args *args = (thread_args*)param;
  void* arg = args->arg;
  void* retval = NULL;
  pthread_fn fn = args->start_routine;
  TlsSetValue(thread_self_tls_index, args);
  pthread_link_thread_args(args);
  retval = fn(arg);
  pthread_unlink_thread_args(args);
  free(args);
  return (DWORD)retval;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
  thread_args* args = (thread_args*)malloc(sizeof(thread_args));
  HANDLE createdThread = CreateThread(NULL, attr ? attr->stack_size : 0, Thread_Function, args, CREATE_SUSPENDED, NULL);
  if (!createdThread)
    return 1;
  args->start_routine = start_routine;
  args->arg = arg;
  args->handle = createdThread;
  args->uninterruptible_section_nesting = 0;
  args->waiting_cond = NULL;
  args->in_safepoint = 0;
  ResumeThread(createdThread);
  if (thread)
    *thread = createdThread;
  return 0;
}

int pthread_equal(pthread_t thread1, pthread_t thread2)
{
  return thread1 == thread2;
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
  return pthread_self_args()->handle;
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

pthread_mutex_t mutex_init_lock;

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

void pthread_checkpoint()
{
  thread_args * args = pthread_self_args();
  if (args->uninterruptible_section_nesting == 0 && !args->in_safepoint) {
    args->in_safepoint = 1;
    pthread_np_safepoint();
    args->in_safepoint = 0;
  }
}

void pthread_np_enter_uninterruptible()
{
  thread_args* args;
  if ((args = pthread_self_args()))
    args->uninterruptible_section_nesting++;
}

void pthread_np_leave_uninterruptible()
{
  thread_args* args;
  if ((args = pthread_self_args())) {
    args->uninterruptible_section_nesting--;
    pthread_checkpoint();
  }
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
  if (*mutex == PTHREAD_MUTEX_INITIALIZER) {
    pthread_mutex_lock(&mutex_init_lock);
    if (*mutex == PTHREAD_MUTEX_INITIALIZER) {
      *mutex = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
      pthread_mutex_init(mutex, NULL);
    }
    pthread_mutex_unlock(&mutex_init_lock);
  }
  pthread_np_enter_uninterruptible();
  EnterCriticalSection(*mutex);
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
  LeaveCriticalSection(*mutex);
  pthread_np_leave_uninterruptible();
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
  pthread_mutex_init(&cv->wakeup_lock, NULL);
  cv->first_wakeup = NULL;
  cv->last_wakeup = NULL;
  cv->alertable = 0;
  cv->get_fn = cv_default_event_get_fn;
  cv->return_fn = cv_default_event_return_fn;
  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cv)
{
  pthread_mutex_destroy(&cv->wakeup_lock);
  return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cv)
{
  int count = 0;
  pthread_mutex_lock(&cv->wakeup_lock);
  while (cv->first_wakeup)
  {
    struct thread_wakeup * w = cv->first_wakeup;
    cv->first_wakeup = w->next;
    SetEvent(w->event);
    ++count;
  }
  cv->last_wakeup = NULL;
  pthread_mutex_unlock(&cv->wakeup_lock);
  return 0;
}

int pthread_cond_signal(pthread_cond_t *cv)
{
  struct thread_wakeup * w;
  pthread_mutex_lock(&cv->wakeup_lock);
  w = cv->first_wakeup;
  if (w) {
    cv->first_wakeup = w->next;
    if (!cv->first_wakeup)
      cv->last_wakeup = NULL;
    SetEvent(w->event);
  }
  pthread_mutex_unlock(&cv->wakeup_lock);
  return 0;
}

void cv_wakeup_add(struct pthread_cond_t* cv, struct thread_wakeup* w)
{
  w->event = cv->get_fn();
  w->next = NULL;
  pthread_mutex_lock(&cv->wakeup_lock);
  if (cv->last_wakeup == w) {
    fprintf(stderr, "cv->last_wakeup == w\n");
    ExitProcess(0);
  }
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
  pthread_mutex_unlock(&cv->wakeup_lock);
}

void cv_wakeup_remove(struct pthread_cond_t* cv, struct thread_wakeup* w)
{
  pthread_mutex_lock(&cv->wakeup_lock);
  {
    if (cv->first_wakeup == w) {
      cv->first_wakeup = w->next;
      if (cv->last_wakeup == w)
        cv->last_wakeup = NULL;
    } else {
      struct thread_wakeup * prev = cv->first_wakeup;
      while (prev && prev->next != w)
        prev = prev->next;
      if (!prev)
        return;
      prev->next = w->next;
      if (cv->last_wakeup == w)
        cv->last_wakeup = prev;
    }
  }
  pthread_mutex_unlock(&cv->wakeup_lock);
}

int pthread_cond_wait(pthread_cond_t * cv, pthread_mutex_t * cs)
{
  struct thread_wakeup w;
  cv_wakeup_add(cv, &w);
  if (cv->last_wakeup->next == cv->last_wakeup) {
    fprintf(stderr, "cv->last_wakeup->next == cv->last_wakeup\n");
    ExitProcess(0);
  }
  if (cv->last_wakeup->next != NULL) {
    fprintf(stderr, "cv->last_wakeup->next != NULL\n");
    ExitProcess(0);
  }
  pthread_self_args()->waiting_cond = cv;
  pthread_mutex_unlock(cs);
  if (cv->alertable) {
    while (WaitForSingleObjectEx(w.event, INFINITE, TRUE) == WAIT_IO_COMPLETION);
  } else {
    WaitForSingleObject(w.event, INFINITE);
  }
  pthread_self_args()->waiting_cond = NULL;
  cv->return_fn(w.event);
  pthread_checkpoint();
  pthread_mutex_lock(cs);
  return 0;
}

int pthread_cond_timedwait(pthread_cond_t * cv, pthread_mutex_t * cs, const struct timespec * abstime)
{
  DWORD rv;
  struct thread_wakeup w;
  cv_wakeup_add(cv, &w);
  if (cv->last_wakeup->next == cv->last_wakeup) {
    fprintf(stderr, "cv->last_wakeup->next == cv->last_wakeup\n");
    ExitProcess(0);
  }
  pthread_self_args()->waiting_cond = cv;
  pthread_mutex_unlock(cs);
  {
    struct timeval cur_tm;
    long sec, msec;
    gettimeofday(&cur_tm, NULL);
    sec = abstime->tv_sec - cur_tm.tv_sec;
    msec = sec * 1000 + abstime->tv_nsec / 1000000 - cur_tm.tv_usec / 1000;
    if (msec < 0)
      msec = 0;
    if (cv->alertable) {
      while ((rv = WaitForSingleObjectEx(w.event, msec, TRUE)) == WAIT_IO_COMPLETION);
    } else {
      rv = WaitForSingleObject(w.event, msec);
    }
  }
  pthread_self_args()->waiting_cond = NULL;
  cv->return_fn(w.event);
  if (rv == WAIT_TIMEOUT)
    cv_wakeup_remove(cv, &w);
  pthread_checkpoint();
  pthread_mutex_lock(cs);
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
  thread_args * args = (thread_args*)malloc(sizeof(thread_args));
  thread_self_tls_index = TlsAlloc();
  args->waiting_cond = NULL;
  args->uninterruptible_section_nesting = 0;
  pthread_all_threads = NULL;
  pthread_mutex_init(&mutex_init_lock, NULL);
  pthread_mutex_init(&pthread_all_threads_lock, NULL);
  DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &args->handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
  TlsSetValue(thread_self_tls_index, args);
  pthread_link_thread_args(args);
}
