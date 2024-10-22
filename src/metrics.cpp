#include "metrics.h"

#include <cmath>

float MSE(const Channel& c1, const Channel& c2) {
  constexpr int kNums = 16;
  float total_error = 0;
  const auto* c1mem = c1.mem();
  const auto* c2mem = c2.mem();
  int i = 0;
  for (; i + kNums <= c1.numel(); i += kNums) {
    float cur_error = 0;
    for (int j = 0; j < kNums; ++j) {
      float diff = c1mem[i + j] - c2mem[i + j];
      cur_error += diff * diff;
    }
    total_error += cur_error;
  }
  float cur_error = 0;
  for (int j = i; j < c1.numel(); ++j) {
    for (int j = 0; j < kNums; ++j) {
      float diff = c1mem[j] - c2mem[j];
      cur_error += diff * diff;
    }
  }
  total_error += cur_error;
  return total_error / c1.numel();
}

constexpr float kMaxPSNR = 60;

float PSNR(float mse, float range) { return std::min<float>(kMaxPSNR, 10 * std::log10(range * range / mse)); }

float PSNR(const Channel& c1, const Channel& c2) { return PSNR(MSE(c1, c2), kBitRange); }

float PSNR(const Image& i1, const Image& i2) {
  auto c1 = i1.extractChannels();
  auto c2 = i2.extractChannels();
  if (c1.size() == 1) {
    return PSNR(c1[0], c2[0]);
  } else {
    return (PSNR(c1[0], c2[0]) * kYChannelWeight + PSNR(c1[1], c2[1]) + PSNR(c1[2], c2[2])) / (kYChannelWeight + 2);
  }
}
