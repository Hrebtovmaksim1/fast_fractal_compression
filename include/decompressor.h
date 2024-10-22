#pragma once

#include <chrono>
#include <vector>

#include "common.h"
#include "image.h"
#include "io.h"
#include "utils.h"

// decompresses one channel
class Decompressor {
public:
  Decompressor(const Metadata& metadata);

  Channel decompress(RStream& stream);

  // applies translations once
  void apply(Channel& dst, const Channel& src) const;
  void reportTimings() const;

private:
  // stores location of each leaf subblock of reference channel, location and brigntess offset of its match
  struct Translation {
    int a_mem_offset_;
    int b_mem_offset_;
    int brightness_;
    Size sz_;
  };

  // loads one max block
  void deserializeNodes(RStream& stream);

  // loads all blocks
  void deserializeNode(RStream&, int level, int block_num, int subblock_num);

  // iteratively applies translations given number of times
  Channel restore(int number_applies = kNumApplies) const;

  const Metadata& metadata_;
  Storage<Size> subblock_sizes_;

  std::vector<Translation> translations_;

  mutable std::chrono::duration<double> deserialization_time_{0};
  mutable std::chrono::duration<double> restore_time_{0};
};
