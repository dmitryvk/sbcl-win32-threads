#include "sbcl.h"

#if defined(LISP_FEATURE_SB_THREAD)
#include "pthreads_win32.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

void odprintf(const char *fmt, ...);

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

DWORD thread_self_tls_index;

typedef unsigned char boolean;

void pthread_np_suspend(pthread_t thread)
{
  CONTEXT context;
  SuspendThread(thread->handle);
  context.ContextFlags = CONTEXT_FULL;
  GetThreadContext(thread->handle, &context);
}

int pthread_np_get_thread_context(pthread_t thread, CONTEXT* context)
{
  context->ContextFlags = CONTEXT_FULL;
  return GetThreadContext(thread->handle, context) != 0;
}

void pthread_np_resume(pthread_t thread)
{
  ResumeThread(thread->handle);
}

unsigned char pthread_np_interruptible(pthread_t thread)
{
  return thread->uninterruptible_section_nesting == 0 && thread->waiting_cond == NULL;
}

void pthread_np_request_interruption(pthread_t thread)
{
  if (thread->waiting_cond) {
    pthread_cond_broadcast(thread->waiting_cond);
  }
}

pthread_t pthread_self()
{
  return (pthread_t)TlsGetValue(thread_self_tls_index);
}

const char * state_to_str(pthread_thread_state state)
{
  switch (state) {
    case pthread_state_running: return "running";
    case pthread_state_finished: return "finished";
    case pthread_state_joined: return "joined";
  }
}

DWORD WINAPI Thread_Function(LPVOID param)
{
  pthread_t self = (pthread_t)param;
  void* arg = self->arg;
  void* retval = NULL;
  pthread_fn fn = self->start_routine;
  TlsSetValue(thread_self_tls_index, self);
  self->retval = fn(arg);
  pthread_mutex_lock(&self->lock);
  self->state = pthread_state_finished;
  odprintf("thread function returned, state is %s", state_to_str(self->state));
  pthread_cond_broadcast(&self->cond);
  while (!self->detached && self->state != pthread_state_joined) {
    pthread_cond_wait(&self->cond, &self->lock);
    odprintf("detached = %d, state = %s", self->detached, state_to_str(self->state));
  }
  pthread_mutex_unlock(&self->lock);
  
  odprintf("destroying the thread");
  
  pthread_mutex_destroy(&self->lock);
  pthread_cond_destroy(&self->cond);
  CloseHandle(self->handle);
  free(self);
  return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
  pthread_t pth = (pthread_t)malloc(sizeof(pthread_thread));
  pthread_t self = pthread_self();
  int i;
  HANDLE createdThread = CreateThread(NULL, attr ? attr->stack_size : 0, Thread_Function, pth, CREATE_SUSPENDED, NULL);
  if (!createdThread)
    return 1;
  pth->start_routine = start_routine;
  pth->arg = arg;
  pth->handle = createdThread;
  pth->uninterruptible_section_nesting = 0;
  pth->waiting_cond = NULL;
  pth->in_safepoint = 0;
  if (self) {
    pth->blocked_signal_set = self->blocked_signal_set;
  } else {
    sigemptyset(&pth->blocked_signal_set);
  }
  for (i = 1; i < NSIG; ++i)
    pth->signal_is_pending[i] = 0;
  pth->state = pthread_state_running;
  pthread_mutex_init(&pth->lock, NULL);
  pthread_cond_init(&pth->cond, NULL);
  pth->detached = 0;
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
  int retval = 0;
  pthread_mutex_lock(&thread->lock);
  if (thread->detached)
    odprintf("thread_detach on 0x%p, already detached", thread);
  thread->detached = 1;
  pthread_cond_broadcast(&thread->cond);
  pthread_mutex_unlock(&thread->lock);
  return retval;
}

int pthread_join(pthread_t thread, void **retval)
{
  odprintf("pthread_join on 0x%p started", thread);
  pthread_mutex_lock(&thread->lock);
  if (thread->detached)
    odprintf("pthread_join on 0x%p, thread is detached", thread);
  odprintf("joining 0x%p, state is %s", thread, state_to_str(thread->state));
  while (thread->state != pthread_state_finished) {
    pthread_cond_wait(&thread->cond, &thread->lock);
    odprintf("woke up joining 0x%p, state is %s", thread, state_to_str(thread->state));
  }
  thread->state = pthread_state_joined;
  pthread_cond_broadcast(&thread->cond);
  if (retval)
    *retval = thread->retval;
  odprintf("done joining 0x%p, state is %s", thread, state_to_str(thread->state));
  pthread_mutex_unlock(&thread->lock);
  odprintf("pthread_join on 0x%p returned", thread);
  return 0;
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
  pthread_t self = pthread_self();
  if (oldset)
    *oldset = self->blocked_signal_set;
  if (set) {
    const char * action;
    
    switch (how) {
      case SIG_BLOCK:
        action = "blocking";
        self->blocked_signal_set |= *set;
        break;
      case SIG_UNBLOCK:
        action = "unblocking";
        self->blocked_signal_set &= ~(*set);
        break;
      case SIG_SETMASK:
        action = "setting";
        self->blocked_signal_set = *set;
        break;
    }
    if (0)
    {
      char buf[100];
      sprintf(buf, "[0x%p] set signals mask to 0x%x by %s of 0x%x", self, self->blocked_signal_set, action, *set);
      OutputDebugString(buf);
    }
  }
  if (set)
  {
    int i;
    for (i = 1; i < NSIG; ++i) {
      if (!sigismember(&self->blocked_signal_set, i)) {
        unsigned int is_pending = InterlockedExchange(&self->signal_is_pending[i], 0);
        if (is_pending) {
          odprintf("calling pending signal handler for signal %d", i);
          pthread_np_pending_signal_handler(i);
        }
      }
    }
  }
  return 0;
}

pthread_mutex_t mutex_init_lock;

int pthread_mutex_init(pthread_mutex_t * mutex, const pthread_mutexattr_t * attr)
{
  *mutex = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
  InitializeCriticalSection(*mutex);
  return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
  return 0;
}
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
  return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr,int mutex_type)
{
  return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
  return 0;
}
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
  return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr,int mutex_type)
{
  return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t* attr)
{
  return 0;
}
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr)
{
  return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t* attr,int mutex_type)
{
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
  if (*mutex != PTHREAD_MUTEX_INITIALIZER)
    DeleteCriticalSection(*mutex);
  return 0;
}

void pthread_np_add_pending_signal(pthread_t thread, int signum)
{
  const char * s = thread->signal_is_pending[signum] ? "pending" : "not pending";
  odprintf("setting signal %d for thread 0x%p to pending (was: %s)", signum, thread, s);
  thread->signal_is_pending[signum] = 1;
}

void pthread_np_remove_pending_signal(pthread_t thread, int signum)
{
  const char * s = thread->signal_is_pending[signum] ? "pending" : "not pending";
  odprintf("setting signal %d for thread 0x%p to not pending (was: %s)", signum, thread, s);
  thread->signal_is_pending[signum] = 0;
}

void pthread_checkpoint()
{
  pthread_t self = pthread_self();
  if (self->uninterruptible_section_nesting == 0 && !self->in_safepoint) {
    self->in_safepoint = 1;
    pthread_np_safepoint();
    self->in_safepoint = 0;
  }
}

void pthread_np_enter_uninterruptible()
{
  pthread_t self = pthread_self();
  if (self)
    self->uninterruptible_section_nesting++;
}

void pthread_np_leave_uninterruptible()
{
  pthread_t self = pthread_self();
  if (self) {
    self->uninterruptible_section_nesting--;
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

int pthread_mutex_trylock(pthread_mutex_t *mutex)
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
  if (TryEnterCriticalSection(*mutex))
    return 0;
  else
    return EBUSY;
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
  if (!cv->first_wakeup)
    odprintf("no waiters");
  while (cv->first_wakeup)
  {
    struct thread_wakeup * w = cv->first_wakeup;
    HANDLE waitevent = w->event;
    cv->first_wakeup = w->next;
    SetEvent(waitevent);
    odprintf("signaled event 0x%p", waitevent);
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
  if (!cv->first_wakeup)
    odprintf("no waiters");
  w = cv->first_wakeup;
  if (w) {
    HANDLE waitevent = w->event;
    cv->first_wakeup = w->next;
    if (!cv->first_wakeup)
      cv->last_wakeup = NULL;
    SetEvent(waitevent);
    odprintf("signaled event 0x%p", waitevent);
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
      if (!prev) {
        pthread_mutex_unlock(&cv->wakeup_lock);
        return;
      }
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
  pthread_self()->waiting_cond = cv;
  pthread_mutex_unlock(cs);
  //odprintf("waiting on event 0x%p", w.event);
  if (cv->alertable) {
    while (WaitForSingleObjectEx(w.event, INFINITE, TRUE) == WAIT_IO_COMPLETION);
  } else {
    WaitForSingleObject(w.event, INFINITE);
  }
  //odprintf("wait returned");
  pthread_self()->waiting_cond = NULL;
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
  pthread_self()->waiting_cond = cv;
  pthread_mutex_unlock(cs);
  {
    struct timeval cur_tm;
    long sec, msec;
    gettimeofday(&cur_tm, NULL);
    sec = abstime->tv_sec - cur_tm.tv_sec;
    msec = sec * 1000 + abstime->tv_nsec / 1000000 - cur_tm.tv_usec / 1000;
    if (msec < 0)
      msec = 0;
    //odprintf("waiting on event 0x%p for %d msec", w.event, msec);
    if (cv->alertable) {
      while ((rv = WaitForSingleObjectEx(w.event, msec, TRUE)) == WAIT_IO_COMPLETION);
    } else {
      rv = WaitForSingleObject(w.event, msec);
    }
  }
  //odprintf("wait returned 0x%lx", rv);
  pthread_self()->waiting_cond = NULL;
  if (rv == WAIT_TIMEOUT)
    cv_wakeup_remove(cv, &w);
  cv->return_fn(w.event);
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

void pthread_lock_structures()
{
  pthread_mutex_lock(&mutex_init_lock);
}

void pthread_unlock_structures()
{
  pthread_mutex_unlock(&mutex_init_lock);
}

void pthreads_win32_init()
{
  pthread_t pth = (pthread_t)malloc(sizeof(pthread_thread));
  thread_self_tls_index = TlsAlloc();
  pth->start_routine = NULL;
  pth->arg = NULL;
  pth->uninterruptible_section_nesting = 0;
  pth->waiting_cond = NULL;
  pth->in_safepoint = 0;
  sigemptyset(&pth->blocked_signal_set);
  {
    int i;
    for (i = 1; i < NSIG; ++i)
      pth->signal_is_pending[i] = 0;
  }
  pthread_mutex_init(&mutex_init_lock, NULL);
  DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &pth->handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
  TlsSetValue(thread_self_tls_index, pth);
}

int sigemptyset(sigset_t *set)
{
  *set = 0;
  return 0;
}

int sigfillset(sigset_t *set)
{
  *set = 0xfffffffful;
  return 0;
}

int sigaddset(sigset_t *set, int signum)
{
  *set |= 1 << signum;
  return 0;
}

int sigdelset(sigset_t *set, int signum)
{
  *set &= ~(1 << signum);
  return 0;
}

int sigismember(const sigset_t *set, int signum)
{
  return (*set & (1 << signum)) != 0;
}
#endif
