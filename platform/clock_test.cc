#include "platform/clock.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include <sched.h>

#include "gtest/gtest.h"

using float_sec = std::chrono::duration<float>;
using milliseconds = std::chrono::milliseconds;

template <typename T>
float_sec sleep_for(T t) {
  using namespace std::chrono;
  auto start = high_resolution_clock::now();
  std::this_thread::sleep_for(t);
  float_sec fsec = high_resolution_clock::now() - start;
  std::cout << "slept for " << (fsec.count() * 1E3) << " ms" << std::endl;
  return fsec;
}

TEST(gthread_clock, thread_clock_unaffected_by_sleep) {
  using namespace gthread;

  auto last = thread_clock::now();
  std::cout << "sizeof(thread_clock::now()) => " << sizeof(last) << std::endl;
  for (int i = 0; i < 10; ++i) {
    auto sleep_time = sleep_for(milliseconds{10});
    auto t = thread_clock::now();
    float_sec elapsed = t - last;
    EXPECT_GT(sleep_time, elapsed);
    std::cout << "elapsed: " << (elapsed.count() * 1E3) << "ms" << std::endl;
    last = t;
  }
}

TEST(gthread_clock, thread_clock_no_drift) {
  using namespace gthread;
  thread_clock::duration acc{0};
  auto last = thread_clock::now();
  while (thread_clock::now() - last < milliseconds{500}) {
    auto start = thread_clock::now();
    for (uint64_t i = 0; i < 1000000; ++i) {
      sched_yield();
    }
    acc += (thread_clock::now() - start);
  }
  EXPECT_GE(thread_clock::now() - last, acc);
}
