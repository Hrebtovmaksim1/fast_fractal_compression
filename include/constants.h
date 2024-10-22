#pragma once

#include <climits>
#include <limits>
#include <utility>

using Size = std::pair<int, int>;
using Offset = std::pair<int, int>;

// Unchangeable constants
constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr int kVecNumel = 8;

constexpr int kBaseBlockLevel = 0;
constexpr Size kBaseBlockSize = std::make_pair(1, 1);

constexpr int kBitsForNumChannels = 2;
constexpr int kBitDepth = CHAR_BIT;

// Changeable constants
constexpr Size kMinimumBlockSize = std::make_pair(2, 2);
constexpr Size kMaximumBlockSize = std::make_pair(32, 32);

constexpr int kSearchStride = 1;
constexpr float kAlpha = 0.75;
constexpr int kBitsPerShape = 11;

constexpr int kYChannelWeight = 4;
constexpr int kNumApplies = 100;

// Checks
constexpr bool isPowerOfTwo(unsigned int x) { return !(x & (x - 1)); }

constexpr int validateBlockSize(Size sz) {
  return isPowerOfTwo(sz.first) && sz.second == sz.first * 2 || sz.second == sz.first;
}

static_assert(validateBlockSize(kMinimumBlockSize));
static_assert(validateBlockSize(kMaximumBlockSize));

static_assert(isPowerOfTwo(kSearchStride));
static_assert(kAlpha > 0 && kAlpha < 1);
static_assert(isPowerOfTwo(kVecNumel));

constexpr int getBlockLevel(Size sz) {
  if (sz == kBaseBlockSize) {
    return kBaseBlockLevel;
  } else if (sz.first == sz.second) {
    return getBlockLevel({sz.first / 2, sz.second}) + 1;
  } else {
    return getBlockLevel({sz.first, sz.second / 2}) + 1;
  }
}

// Calculated constants

constexpr Size getBlockSize(int level) {
  if (level == kBaseBlockLevel) {
    return kBaseBlockSize;
  }
  auto sz = getBlockSize(level - 1);
  if (sz.first == sz.second) {
    return {sz.first, sz.second * 2};
  } else {
    return {sz.first * 2, sz.second};
  }
}

constexpr int getBlockNumel(int level) {
  auto sz = getBlockSize(level);
  return sz.first * sz.second;
}

constexpr int kMinBlockLevel = getBlockLevel(kMinimumBlockSize);
constexpr int kMaxBlockLevel = getBlockLevel(kMaximumBlockSize);

constexpr int kNumBlockLevels = kMaxBlockLevel - kMinBlockLevel + 1;

constexpr int kMinBlockNumel = getBlockNumel(kMinBlockLevel);
constexpr int kMaxBlockNumel = getBlockNumel(kMaxBlockLevel);

constexpr int kMinBlocksInMax = kMaxBlockNumel / kMinBlockNumel;

constexpr int kVecBytes = kVecNumel * sizeof(float);

constexpr int kMaxShape = 1u << kBitsPerShape;

constexpr int kBitRange = 1u << kBitDepth;
constexpr int kRangeOffset = kBitDepth + 1;
