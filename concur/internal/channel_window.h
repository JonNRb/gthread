#pragma once

#include <mutex>

#include "absl/types/optional.h"
#include "sched/sched.h"
#include "sched/task.h"

namespace gthread {
namespace internal {
template <typename T>
class channel_window {
 public:
  channel_window() : _waiter(nullptr), _reader(nullptr), _closed(false) {}

  constexpr bool is_closed() const { return _closed; }

  void close();

  template <typename U>
  absl::optional<T> write(U&& t);

  absl::optional<T> read();

 private:
  // XXX: assumes no smp and the scheduler can be locked in the implementation
  // during crossover
  task* _waiter;
  absl::optional<T>* _reader;
  std::unique_lock<sched> _hot_potato;
  bool _closed;
};
}  // namespace internal
}  // namespace gthread

#include "concur/internal/channel_window_impl.h"
