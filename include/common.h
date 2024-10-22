#pragma once

#include <vector>

#include "utils.h"

// blocks' base offsets in maximum block
class Pattern {
public:
  Pattern(Size sz);

  const auto& operator[](int level) const { return pattern_[level]; }

  auto sz() const { return sz_; }

private:
  // recursively builds pattern
  void build(int level, int start, Offset ofs);

  Size sz_;
  Storage<std::vector<int>> pattern_;
};

// common data based on image size that is used during both compression and decompression
struct Metadata {
  Metadata(Size sz);

  Size sz_;
  Pattern pt_;

  int num_a_blocks_;
  int num_a_groups_;

  int num_b_blocks_;
  int num_b_groups_;

  std::vector<int> a_block_offsets_;
  std::vector<int> b_block_offsets_;

  int bits_for_match_idx_;

  Storage<int> level_offsets_;
  Storage<int> num_min_blocks_in_level_;
};
