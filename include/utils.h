#pragma once

#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <immintrin.h>
#include "constants.h"

template <typename T>
class Storage {
public:
  T& operator[](int level) { return st_[level - kMinBlockLevel]; }

  const T& operator[](int level) const { return st_[level - kMinBlockLevel]; }

private:
  std::array<T, kNumBlockLevels> st_;
};

typedef float Vec __attribute__((vector_size(kVecBytes)));
typedef int IVec __attribute__((vector_size(kVecBytes)));

inline void vecArgmin(Vec& __restrict__ base, IVec& __restrict__ base_arg, Vec update, int arg) {
  for (int i = 0; i < kVecNumel; ++i) {
    if (update[i] < base[i]) {
      base[i] = update[i];
      base_arg[i] = arg;
    }
  }
}

template <typename VType>
using VecHolder = std::unique_ptr<VType, std::function<void(VType*)>>;

template <typename VType = Vec>
VecHolder<VType> allocVecs(int n) {
  VType* ptr = static_cast<VType*>(std::aligned_alloc(kVecBytes, kVecBytes * n));
  auto deleter = [](VType* ptr) { std::free(ptr); };
  return VecHolder<VType>(ptr, deleter);
}

inline void assertWithMessage(bool condition, const std::string& msg) {
  if (!condition) [[unlikely]] {
    std::cout << msg << '\n';
  }
}
