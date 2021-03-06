#include "arch/switch_to.h"

#include <ucontext.h>
#include <condition_variable>
#include <csetjmp>
#include <thread>

#include <benchmark/benchmark.h>

#include "platform/memory.h"

static gthread_saved_ctx_t main_ctx;
static gthread_saved_ctx_t test_func_ctx;

static void test_func(void* _) {
  while (true) {
    __asm__ __volatile__("" ::: "memory");
    gthread_switch_to(&test_func_ctx, &main_ctx);
  }
}

static void bench_gthread_switch_to(benchmark::State& state) {
  void* stack;
  size_t stack_size;
  gthread::allocate_stack(gthread::k_default_attr, &stack, &stack_size);

  gthread_switch_to_and_spawn(&main_ctx, stack, test_func, NULL);

  for (auto _ : state) gthread_switch_to(&main_ctx, &test_func_ctx);

  gthread::free_stack((char*)stack - stack_size, stack_size);
}

BENCHMARK(bench_gthread_switch_to);

static ucontext_t main_uctx;
static ucontext_t utest_func_uctx;

static void utest_func() {
  while (true) {
    __asm__ __volatile__("" ::: "memory");
    swapcontext(&utest_func_uctx, &main_uctx);
  }
}

static void bench_ucontext_swapcontext(benchmark::State& state) {
  void* stack;
  size_t stack_size;

  gthread::allocate_stack(gthread::k_default_attr, &stack, &stack_size);

  if (getcontext(&utest_func_uctx)) {
    perror("getcontext()");
    abort();
  }
  utest_func_uctx.uc_stack.ss_sp = &((char*)stack)[-stack_size];
  utest_func_uctx.uc_stack.ss_size = stack_size;
  utest_func_uctx.uc_link = &main_uctx;

  makecontext(&utest_func_uctx, utest_func, 0);

  for (auto _ : state) swapcontext(&main_uctx, &utest_func_uctx);

  gthread::free_stack((char*)stack - stack_size, stack_size);
}

BENCHMARK(bench_ucontext_swapcontext);

std::jmp_buf jtest_func_buf;
std::jmp_buf main_buf;

static void jtest_func() {
  if (setjmp(jtest_func_buf)) {
    longjmp(main_buf, 1);
  }
}

static void bench_setjmp_longjmp(benchmark::State& state) {
  if (setjmp(main_buf) == 0) {
    jtest_func();
  }

  for (auto _ : state) {
    if (setjmp(main_buf) == 0) {
      longjmp(jtest_func_buf, 1);
    }
  }

  if (setjmp(main_buf) == 0) {
    longjmp(jtest_func_buf, 0);
  }
}

BENCHMARK(bench_setjmp_longjmp);

static void bench_mutex_condition(benchmark::State& state) {
  std::mutex mu;
  std::condition_variable cv;
  bool done_flag = false;

  std::thread bg([&mu, &cv, &done_flag]() {
    while (!done_flag) {
      std::unique_lock<std::mutex> l(mu);
      cv.notify_one();
      cv.wait(l);
    }
  });

  for (auto _ : state) {
    std::unique_lock<std::mutex> l(mu);
    cv.notify_one();
    cv.wait(l);
  }

  done_flag = true;
  cv.notify_one();
  bg.join();
}

BENCHMARK(bench_mutex_condition);

BENCHMARK_MAIN()
