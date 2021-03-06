#pragma once

#include <functional>
#include <memory>
#include <tuple>

namespace gthread {
/**
 * designed to be single use (to start tasks that take move-only parameters)
 */
template <typename Function, typename... Args>
class function_marshall {
 public:
  function_marshall(Function&& f, Args&&... args);

  typename std::result_of<Function(Args...)>::type operator()();

 private:
  bool _used;
  Function _function;
  std::tuple<Args...> _args;
};

template <typename Function, typename... Args>
auto make_unique_function_marshall(Function&& f, Args&&... args) {
  return std::make_unique<function_marshall<Function, Args...>>(
      std::forward<Function>(f), std::forward<Args>(args)...);
}
}  // namespace gthread

#include "util/function_marshall_impl.h"
