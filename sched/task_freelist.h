#pragma once

#include <list>

#include "gthread.h"
#include "sched/task.h"

namespace gthread {
// TODO(jonnrb): match stack size to `gthread::attr`
class task_freelist {
 public:
  explicit task_freelist(unsigned max_size) : _max_size(max_size) {}

  task* make_task(const attr& a);

  void return_task(task* t);

 private:
  std::list<task*> _l;
  unsigned _max_size;
};
}  // namespace gthread