#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#include "sched/sched.h"

void* important_task(void* arg) {
  const char* msg = (const char*)arg;
  clock_t start = clock();
  for (clock_t c = clock();; c = clock()) {
    if ((c - start) > CLOCKS_PER_SEC) {
      printf("hi I am task \"%c\"\n", *msg);
      start = c;
    } else {
      gthread_sched_yield();
    }
  }
}

int init() {
  gthread_sched_handle_t tasks[26];
  char msgs[26] = {'A'};
  for (int i = 0; i < 26; ++i) {
    printf("creating task %d\n", i);
    msgs[i] = msgs[0] + i;
    assert(!gthread_sched_spawn(&tasks[i], NULL, important_task,
                                (void*)(&msgs[i])));
  }
  printf("done and stuff\n");
  while (true) gthread_sched_yield();
  return 0;
}

int main() {
  gthread_sched_init();
  printf("hi!!!\n");
  return init();
}
