#include "sched/sched_node.h"

#include <mutex>
#include <thread>

#include "util/log.h"

namespace gthread {
namespace {
thread_local sched_node* g_current_sched_node = nullptr;
}  // namespace

sched_node* sched_node::current() { return g_current_sched_node; }

void sched_node::start_async() mt_no_analysis {
  g_current_sched_node = this;

  _last_tick = vruntime_clock::now();
  _host_task.wrap_current();

  bool expected = false;
  if (!_running.compare_exchange_strong(expected, true)) {
    gthread_log_fatal("sched_node was already running");
  }
}

void sched_node::start() mt_no_analysis {
  g_current_sched_node = this;

  _last_tick = vruntime_clock::now();
  _host_task.wrap_current();

  bool expected = false;
  if (!_running.compare_exchange_strong(expected, true)) {
    gthread_log_fatal("sched_node was already running");
  }

  _host_task.run_state = task::WAITING;
  yield();  // will not resume until stopped

  if (_running.load()) {
    gthread_log_fatal(
        "control resumed in host task before sched_node stopped and sched_node "
        "was started synchronously. there is either a bug in the scheduler or "
        "there was nothing initially scheduled when start() was called (user "
        "error)");
  }
}

template <typename T>
class unlock_guard {
  T& _t;

 public:
  unlock_guard(T& t) : _t(t) { _t.unlock(); }
  ~unlock_guard() { _t.lock(); }
};

task* sched_node::get_next_task(task* cur) {
  // update virtual runtime of currently running task
  cur->vruntime += std::chrono::duration_cast<decltype(cur->vruntime)>(
      vruntime_clock::now() - _last_tick);

  // if the task was in a runnable state when the scheduler was invoked, push
  // it to the runqueue
  if (cur->run_state == task::RUNNING) {
    _rq.push(cur);
  } else if (cur->run_state == task::STOPPED && cur->detached) {
    _deferred = cur;  // defer `return_task()` to avoid freeing this stack
  }

  // while idling, unlock `_spin_lock` and sleep the kernel-managed thread
  auto* next_task = _rq.pop([this](auto&& wake_time) mt_no_analysis {
    unlock_guard<spin_lock> u(_spin_lock);
    std::this_thread::sleep_until(wake_time);
  });

  _last_tick = vruntime_clock::now();
  return next_task;
}

void sched_node::yield() {
  if (!_running.load()) return;

  guard l(_spin_lock);

  // if we deferred returning a task, do it now
  if (_deferred != nullptr) {
    _task_freelist->return_task(_deferred);
    _deferred = nullptr;
  }

  auto* cur = task::current();
  auto* next_task = get_next_task(cur);

  if (next_task != cur) {
    unlock_guard<spin_lock> u(_spin_lock);
    next_task->switch_to([this]() { g_current_sched_node = this; });
  }
}

void sched_node::yield_for(internal::rq::sleepqueue_clock::duration duration) {
  auto* current = task::current();

  {
    guard l(_spin_lock);
    current->run_state = task::WAITING;
    _rq.sleep_push(current, duration);
  }

  yield();
}

bool sched_node::spin_lock::try_lock() {
  bool expected = false;
  return _flag.compare_exchange_strong(expected, true,
                                       std::memory_order_acquire);
}

void sched_node::spin_lock::lock() {
  // low level spin utilizing pause instruction between tries
  bool expected = false;
  while (
      !_flag.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
    asm("pause");
    expected = false;
  }
}

void sched_node::spin_lock::unlock() {
  _flag.store(false, std::memory_order_release);
}

void sched_node::schedule(task* t) {
  guard l(_spin_lock);

  // initialize the vruntime if |t| is a new task
  if (t->vruntime == std::chrono::microseconds{0}) {
    t->vruntime = _rq.min_vruntime();
  }

  _rq.push(t);
}

void sched_node::switch_to(task* t) {
  {
    std::lock_guard<spin_lock> l(_spin_lock);
    auto now = vruntime_clock::now();
    auto* cur = task::current();
    cur->vruntime +=
        std::chrono::duration_cast<decltype(cur->vruntime)>(now - _last_tick);
    _last_tick = now;
  }

  t->switch_to([this]() { g_current_sched_node = this; });
}
}  // namespace gthread
