#define _GNU_SOURCE

#include "platform/tls.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "arch/atomic.h"
#include "util/compiler.h"

/**
 * TLS ABI document: https://www.akkadia.org/drepper/tls.pdf
 *
 * XNU says "ya know who really cares what everyone else does" and uses the
 * %gs segment for tls data on x86-64. k.
 *
 * however, what is nifty is that if a slot is NULL but is a valid TLS module,
 * it will be dynamically allocated.
 */

// https://github.com/apple/darwin-libpthread/blob/master/src/internal.h
#define k_num_slots 768

#define k_num_copied_slots 256

// slots 6 and 11 are reserved for WINE (so we're gonna steal em)
#define k_thread_slot_a 6
#define k_thread_slot_b 11

// nicey clang attribute allows using gs segment relative pointer vars
#define gs_relative __attribute__((address_space(256)))

// more portable gs relative (gcc should really get on this)
#define gthread_get_thread_vector_offset(offset)          \
  ({                                                      \
    void *_r;                                             \
    __asm__("mov %%gs:%P1, %0" : "=r"(_r) : "i"(offset)); \
    _r;                                                   \
  })

typedef void *gs_relative *tls_slot_t;

static tls_slot_t g_pthread_self_slot = (tls_slot_t)0;
static tls_slot_t g_thread_slot_a = (tls_slot_t)k_thread_slot_a;
static tls_slot_t g_thread_slot_b = (tls_slot_t)k_thread_slot_b;

/**
 * so xnu doesn't document its syscalls so good
 *
 * actual syscall: https://gist.github.com/aras-p/5389747
 * thanks @aras-p you da guy
 *
 * this function is used by libpthread though so it can be imported
 */
extern void _thread_set_tsd_base(void *);

// this is terrible on several levels, but mono does it so it's okay
static inline size_t get_pthread_slots_offset() {
  static size_t pthread_t_size = 0;
  if (pthread_t_size != 0) return pthread_t_size;

  void *pthread_self = *g_pthread_self_slot;
  uint64_t *search = (uint64_t *)pthread_self;
  while (*(void **)search != pthread_self) ++search;

  return pthread_t_size = (char *)search - (char *)pthread_self;
}

gthread_tls_t gthread_tls_allocate(size_t *tls_image_reserve,
                                   size_t *tls_image_alignment) {
  size_t pthread_t_size = get_pthread_slots_offset();

  void *tls = calloc(k_num_slots * sizeof(void *) + pthread_t_size, 1);
  if (memcpy(tls, *g_pthread_self_slot, pthread_t_size) == NULL) {
    free(tls);
    return NULL;
  }

  // set the user tcb to be this tls block
  void **tls_slots = (void **)((char *)tls + pthread_t_size);
  tls_slots[0] = tls;

  if (tls_image_reserve != NULL) *tls_image_reserve = 0;
  if (tls_image_alignment != NULL) *tls_image_reserve = 0;

  return (gthread_tls_t)tls;
}

void gthread_tls_free(gthread_tls_t tls) {
  if (tls == NULL) return;

  void **tls_slots = (void **)((char *)tls + get_pthread_slots_offset());
  for (int i = k_num_copied_slots; i < k_num_slots; ++i)
    if (tls_slots[i] != NULL) free(tls_slots[i]);

  free(tls);
}

gthread_tls_t gthread_tls_current() {
  return (gthread_tls_t)*g_pthread_self_slot;
}

void gthread_tls_set_thread(gthread_tls_t tls, void *thread) {
  void **tls_slots = (void **)((char *)tls + get_pthread_slots_offset());
  tls_slots[k_thread_slot_a] = thread;
}

void *gthread_tls_get_thread(gthread_tls_t tls) {
  void **tls_slots = (void **)((char *)tls + get_pthread_slots_offset());
  return tls_slots[k_thread_slot_a];
}

void *gthread_tls_current_thread() { return *g_thread_slot_a; }

void gthread_tls_use(gthread_tls_t tls) {
  _thread_set_tsd_base((void *)((char *)tls + get_pthread_slots_offset()));
}

// nop: macOS does this on the fly
int gthread_tls_initialize_image(gthread_tls_t tls) { return 0; }