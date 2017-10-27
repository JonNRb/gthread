#pragma once

#include <atomic>
#include <chrono>
#include <functional>

#include "arch/switch_to.h"
#include "gthread.h"
#include "platform/tls.h"
#include "util/compiler.h"

namespace gthread {

struct task {
 public:
  // constructs a stack and sets default values
  task(const attr& a);

  ~task();

  int reset();

  /**
   * quickly starts task and switches back to the caller. meant to just get
   * things primed. it is undefined (like reaaallly undefined) as to what
   * happens when a task is switched to that hasn't been `start()`ed.
   */
  int start();

  /**
   * convenience function to call the task's end handler. will assert that
   * the end handler does *not* return. this function will not return.
   */
  void stop(void* return_value);

  /**
   * always returns a pointer to the current task
   */
  static inline task* current();

  /**
   * suspends the currently running task and switches to `this`
   */
  int switch_to();

  using time_slice_trap = std::function<task*(task*)>;

  template <typename Duration>
  static void set_time_slice_trap(const time_slice_trap& trap,
                                  Duration interval);

  using end_handler = std::function<void(task*)>;

  static void set_end_handler(end_handler handler);

 public:
  gthread_tls_t tls;

  std::atomic<task*> joiner;

  typedef enum { RUNNING, SUSPENDED, STOPPED, WAITING } run_state_t;
  run_state_t run_state;

  typedef void* entry_t(void*);
  entry_t* entry;
  void* arg;
  void* return_value;

  gthread_saved_ctx_t ctx;
  void* stack;
  size_t total_stack_size;

  std::chrono::microseconds vruntime;

  uint64_t priority_boost;

 private:
  // default constructor only for root task
  task();

  void record_time_slice(std::chrono::microseconds elapsed);

  void reset_timer_and_record_time();

  int switch_to_internal(std::chrono::microseconds* elapsed);

  static void time_slice_trap_base(std::chrono::microseconds elapsed);

  static void init();

  // task representing the initial context of execution
  static task _root_task;
  static std::atomic<bool> _is_root_task_init;

  static end_handler _end_handler;

  // task switching MUST not be reentrant
  static std::atomic_flag _lock;

  static bool _timer_enabled;
  static time_slice_trap _time_slice_trap;
};

}  // namespace gthread

#include "sched/task_inline.h"
