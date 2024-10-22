#include <chrono>

#include "compressor.h"
#include "decompressor.h"

int clamp(float f) {
  int i = f + 0.5;
  i = std::min(i, kBitRange - 1);
  i = std::max(i, 0);
  return i;
}

void Compressor::serializeNode(WStream& stream, int level, int vnum, int vpos, int ipos, int num_leafs) {
  auto dump = [&] {
    int br = clamp(a_mean()[vnum * kMinBlocksInMax * 2 + metadata_.level_offsets_[level] + ipos][vpos]);
    stream.dump(br, kBitDepth);
    stream.dump(block_matches_indices_.get()[vnum * kMinBlocksInMax * 2 + metadata_.level_offsets_[level] + ipos][vpos],
                metadata_.bits_for_match_idx_);
  };

  if (level == kMinBlockLevel) {
    dump();
  } else {
    if (num_leafs == 0) {
      stream.dump(0, 1);
      dump();
    } else {
      stream.dump(1, 1);
      int l_num_leafs =
          coverings_num_leafs_in_left_[level]
              .get()[vnum * kMinBlocksInMax + ipos * metadata_.num_min_blocks_in_level_[level] + num_leafs][vpos];
      int r_num_leafs = num_leafs - l_num_leafs - 1;
      serializeNode(stream, level - 1, vnum, vpos, ipos * 2, l_num_leafs);
      serializeNode(stream, level - 1, vnum, vpos, ipos * 2 + 1, r_num_leafs);
    }
  }
}

void Compressor::serializeNodes(WStream& stream, const std::vector<int>& leafs_per_block) {
  for (int block_num = 0; block_num < metadata_.num_a_blocks_; ++block_num) {
    int vnum = block_num / kVecNumel;
    int vpos = block_num % kVecNumel;
    int nleafs = leafs_per_block[block_num];
    serializeNode(stream, kMaxBlockLevel, vnum, vpos, 0, nleafs);
  }
}

void Decompressor::deserializeNodes(RStream& stream) {
  auto start = std::chrono::high_resolution_clock::now();
  for (int block_num = 0; block_num < metadata_.num_a_blocks_; ++block_num) {
    deserializeNode(stream, kMaxBlockLevel, block_num, 0);
  }
  auto end = std::chrono::high_resolution_clock::now();
  deserialization_time_ = std::chrono::duration<double>(end - start);
}

void Decompressor::deserializeNode(RStream& stream, int level, int block_num, int subblock_num) {
  auto extract = [&] {
    int brightness = stream.extract(kBitDepth);
    int match = stream.extract(metadata_.bits_for_match_idx_);
    int a_mem_offset = metadata_.a_block_offsets_[block_num] + metadata_.pt_[level][subblock_num];
    int b_mem_offset = metadata_.b_block_offsets_[match] + metadata_.pt_[level][subblock_num];
    translations_.push_back(Translation{a_mem_offset, b_mem_offset, brightness, subblock_sizes_[level]});
  };

  if (level == kMinBlockLevel) {
    extract();
  } else {
    bool split = stream.extract(1);
    if (!split) {
      extract();
    } else {
      deserializeNode(stream, level - 1, block_num, subblock_num * 2);
      deserializeNode(stream, level - 1, block_num, subblock_num * 2 + 1);
    }
  }
}
