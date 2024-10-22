#include "common.h"

Pattern::Pattern(Size sz) : sz_{sz} {
  for (int level = kMaxBlockLevel; level >= kMinBlockLevel; --level) {
    pattern_[level].resize(kMaxBlockNumel / getBlockNumel(level));
  }
  Offset start_offset{0, 0};
  build(kMaxBlockLevel, 0, start_offset);
}

void Pattern::build(int level, int block_num, Offset ofs) {
  pattern_[level][block_num] = ofs.first * sz_.second + ofs.second;
  if (level == kMinBlockLevel) {
    return;
  }
  auto cur_sz = getBlockSize(level);
  auto prev_sz = getBlockSize(level - 1);

  build(level - 1, block_num * 2, ofs);
  build(level - 1, block_num * 2 + 1,
        Offset{ofs.first + cur_sz.first - prev_sz.first, ofs.second + cur_sz.second - prev_sz.second});
}

Metadata::Metadata(Size sz) : sz_{sz}, pt_{sz} {
  for (int h = 0; h < sz.first; h += kMaximumBlockSize.first) {
    for (int w = 0; w < sz.second; w += kMaximumBlockSize.second) {
      int offset = h * sz.second + w;
      a_block_offsets_.push_back(offset);
    }
  }
  for (int h = 0; h < sz.first / 2; h += kSearchStride) {
    for (int w = 0; w < sz.second / 2; w += kSearchStride) {
      int offset = h * sz.second + w;
      b_block_offsets_.push_back(offset);
    }
  }

  num_a_blocks_ = a_block_offsets_.size();
  num_b_blocks_ = b_block_offsets_.size();

  auto extend = [](std::vector<int>& v) {
    while (v.size() % kVecNumel > 0) {
      v.push_back(v.back());
    }
  };

  int match_idx_range = 1;
  int bits_for_match_idx = 0;
  while (match_idx_range < b_block_offsets_.size()) {
    ++bits_for_match_idx;
    match_idx_range *= 2;
  }

  bits_for_match_idx_ = bits_for_match_idx;

  extend(a_block_offsets_);
  extend(b_block_offsets_);

  num_a_groups_ = a_block_offsets_.size() / kVecNumel;
  num_b_groups_ = b_block_offsets_.size() / kVecNumel;

  int level_offset = 0;
  for (int level = kMinBlockLevel; level <= kMaxBlockLevel; ++level) {
    level_offsets_[level] = level_offset;
    level_offset += kMaxBlockNumel / getBlockNumel(level);
    num_min_blocks_in_level_[level] = getBlockNumel(level) / kMinBlockNumel;
  }
}
