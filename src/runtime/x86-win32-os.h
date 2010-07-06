#ifndef _X86_WIN32_OS_H
#define _X86_WIN32_OS_H

#if defined(LISP_FEATURE_SB_THREAD)
typedef struct os_context_t {
  CONTEXT* win32_context;
  sigset_t sigmask;
} os_context_t;
#else
typedef CONTEXT os_context_t;
#endif

typedef long os_context_register_t;

static inline os_context_t *arch_os_get_context(void **void_context)
{
    return (os_context_t *) *void_context;
}

unsigned long os_context_fp_control(os_context_t *context);
void os_restore_fp_control(os_context_t *context);

#endif /* _X86_WIN32_OS_H */
