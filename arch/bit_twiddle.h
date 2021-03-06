#pragma once

#include <cstdint>

static inline uint64_t right_mask(uint64_t bits) {
  uint64_t leading_zeroes;
  __asm__("lzcntq %1, %0\n" : "=r"(leading_zeroes) : "r"(bits));
  return (~(uint64_t)0) >> leading_zeroes;
}
