/**
 * author: JonNRb <jonbetti@gmail.com>
 * license: MIT
 * file: @gthread//platform/memory_linux.c
 * info: implements Linux specific functionality
 */

#include "platform/memory.h"

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#include "arch/bit_twiddle.h"
#include "gthread.h"
#include "util/compiler.h"

namespace gthread {

const size_t k_stack_min = 0x2000;

// same as nptl
static constexpr size_t k_stack_default = 2 << 20;

static inline size_t page_size() {
  static size_t k = 0;
#ifdef _SC_PAGESIZE
  if (k == 0) k = (size_t)sysconf(_SC_PAGESIZE);
#elif defined(PAGE_SIZE)
  if (k == 0) k = (size_t)sysconf(PAGE_SIZE);
#else
#error "no PAGESIZE macro for this system! is this a POSIX system?"
#endif
  return k;
}

// use mmap() to allocate a stack and mprotect() for the guard page
int allocate_stack(const attr& attrs, void** stack, size_t* total_stack_size) {
  static gthread_attr_t defaults = {{k_stack_min, k_stack_default, nullptr}};
  attrs = attrs ?: &defaults;

  // user provided stack space. no guard pages setup in this case.
  if (attrs->stack.addr != nullptr) {
    if (branch_unexpected(((uintptr_t)attrs->stack.addr % page_size()) != 0)) {
      return -1;
    }
    *stack = attrs->stack.addr;
    return 0;
  }

  size_t t;
  if (total_stack_size == nullptr) total_stack_size = &t;
  *total_stack_size = attrs->stack.size + attrs->stack.guardsize;

  // mmap the region as a stack using MAP_GROWSDOWN magic
  void* stackaddr = mmap(NULL, *total_stack_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);

  if (branch_unexpected(stackaddr == NULL)) {
    return -1;
  }

  // the guard page is at the lowest address. apply protection to segv there.
  if (attrs->stack.guardsize != 0) {
    size_t guardsize = a.stack.guardsize != static_cast<size_t>(-1)
                           ? a.stack.guardsize
                           : k_stack_min;
    if (branch_unexpected(mprotect(stackaddr, guardsize, PROT_NONE))) {
      return -1;
    }
  }
  *stack = (void*)((char*)stackaddr + *total_stack_size);
  return 0;
}

int free_stack(void* stack_base, size_t total_stack_size) {
  return munmap((char*)stack_base, total_stack_size);
}

}  // namespace gthread
