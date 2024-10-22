#include <chrono>
#include <iostream>

#include "compressor.h"

// in this file group refers to a set of blocks of size VecNumel

// transforms channel into groups of maximum blocks
void reorder(Vec* __restrict__ group, const float* __restrict__ mem, Vec* __restrict__ mean, Vec* __restrict__ sumsq,
             const Pattern& pattern, const IVec& mem_offsets) {
  const auto& pt = pattern[kMinBlockLevel];
  int ptr = 0;
  for (int block_num = 0; block_num < kMinBlocksInMax; ++block_num) {
    Vec cmean{0};
    Vec tmp[kMinBlockNumel]{0};
    int mbpos = 0;
    for (int h = 0; h < kMinimumBlockSize.first; ++h) {
      for (int w = 0; w < kMinimumBlockSize.second; ++w) {
        int mem_offset = pt[block_num] + h * pattern.sz().second + w;
        for (int v = 0; v < kVecNumel; ++v) {
          tmp[mbpos][v] = mem[mem_offsets[v] + mem_offset];
        }
        cmean += tmp[mbpos];
        ++mbpos;
      }
    }
    cmean /= kMinBlockNumel;
    mean[block_num] = cmean;
    Vec csumsq{0};
    for (int mbpos = 0; mbpos < kMinBlockNumel; ++mbpos) {
      Vec normalized = tmp[mbpos] - cmean;
      group[block_num * kMinBlockNumel + mbpos] = normalized;
      csumsq += normalized * normalized;
    }
    sumsq[block_num] = csumsq;
  }
  int cur_size = kMinBlocksInMax / 2;
  int prev_offset = 0;
  int cur_offset = kMinBlocksInMax;

  while (cur_size > 0) {
    for (int i = 0; i < cur_size; ++i) {
      int l = i * 2;
      int r = i * 2 + 1;
      Vec res = (mean[prev_offset + l] + mean[prev_offset + r]) / 2;
      mean[cur_offset + i] = res;
    }
    prev_offset = cur_offset;
    cur_offset += cur_size;
    cur_size /= 2;
  }
}

// finds matches for minimal blocks
void matchGroup(const Vec* __restrict__ a_group, const Vec* __restrict__ b_group, Vec* __restrict__ asumsq,
                Vec* __restrict__ bsumsq, Vec* __restrict__ buf) {
  for (int block_num = 0; block_num < kMinBlocksInMax; ++block_num) {
    Vec tmp[kVecNumel]{0};
    for (int mbpos = 0; mbpos < kMinBlockNumel; ++mbpos) {
      for (int vpos = 0; vpos < kVecNumel; ++vpos) {
        auto av = a_group[block_num * kMinBlockNumel + mbpos];
        auto bf = b_group[block_num * kMinBlockNumel + mbpos][vpos];
        tmp[vpos] -= 2 * av * bf;
      }
    }
    for (int vpos = 0; vpos < kVecNumel; ++vpos) {
      Vec last = tmp[vpos] + asumsq[block_num] + bsumsq[block_num][vpos];
      buf[block_num * kVecNumel + vpos] = last;
    }
  }
}


inline void updateGroup(const Vec* __restrict__ buf, Vec& __restrict__ errors, IVec& __restrict__ matches,
                        IVec update_matches) {
  Vec cur_errors = errors;
  IVec cur_matches = matches;
  // somehow if these two loops are combined gcc optimizes this poorly and performance drops 4x
  // that is the only reason
  for (int vpos = 0; vpos < 4; ++vpos) {
    vecArgmin(cur_errors, cur_matches, buf[vpos], update_matches[vpos]);
  }
  for (int vpos = 4; vpos < 8; ++vpos) {
    vecArgmin(cur_errors, cur_matches, buf[vpos], update_matches[vpos]);
  }
  errors = cur_errors;
  matches = cur_matches;
}

// finds best coverings for blocks for each layer by combining best coverings from prev layer
void reduce(Vec* __restrict__ buf, Vec* __restrict__ errors, IVec* __restrict__ matches, const IVec update_matches,
            const Vec* __restrict__ a_mean, const Vec* __restrict__ b_mean) {
  int num_blocks = kMinBlocksInMax / 2;
  int cur_offset = 0;
  float mul = kMinBlockNumel / 2;
  while (num_blocks > 0) {
    for (int block_num = 0; block_num < num_blocks; ++block_num) {
      int l = block_num * 2;
      int r = block_num * 2 + 1;
      updateGroup(buf + l * kVecNumel, errors[cur_offset + l], matches[cur_offset + l], update_matches);
      updateGroup(buf + r * kVecNumel, errors[cur_offset + r], matches[cur_offset + r], update_matches);
      for (int vpos = 0; vpos < kVecNumel; ++vpos) {
        Vec nmean1 = a_mean[cur_offset + l] - b_mean[cur_offset + l][vpos];
        Vec nmean2 = a_mean[cur_offset + r] - b_mean[cur_offset + r][vpos];
        Vec diff = nmean1 - nmean2;
        Vec res = buf[l * kVecNumel + vpos] + buf[r * kVecNumel + vpos] + diff * diff * mul;
        buf[block_num * kVecNumel + vpos] = res;
      }
    }
    cur_offset += num_blocks * 2;
    mul *= 2;
    num_blocks /= 2;
  }
  updateGroup(buf, errors[cur_offset], matches[cur_offset], update_matches);
}

void Compressor::matchBlocks() {
  IVec mem_offsets{0};
  const float* __restrict__ a_mem = a_chl_.mem();
  const float* __restrict__ b_mem = b_chl_.mem();
  auto buf = allocVecs(kMinBlocksInMax * kVecNumel);

  for (int vnum = 0; vnum < metadata_.num_a_groups_; ++vnum) {
    IVec group_offsets;
    for (int vpos = 0; vpos < kVecNumel; ++vpos) {
      group_offsets[vpos] = metadata_.a_block_offsets_[vnum * kVecNumel + vpos];
    }
    reorder(a_groups() + vnum * kMaxBlockNumel, a_mem, a_mean() + vnum * kMinBlocksInMax * 2,
            a_sumsq() + vnum * kMinBlocksInMax, metadata_.pt_, group_offsets);
  }
  for (int i = 0; i < metadata_.num_a_groups_ * kMinBlocksInMax * 2; ++i) {
    for (int j = 0; j < kVecNumel; ++j) {
      block_errors_.get()[i][j] = kInf;
      block_matches_indices_.get()[i][j] = 0;
    }
  }
  std::chrono::duration<double> reorder_time{0}, match_time{0}, reduce_time{0};
  IVec b_block_nums{};
  for (int i = 0; i < kVecNumel; ++i) {
    b_block_nums[i] = i;
  }
  for (int vnum = 0; vnum < metadata_.num_b_groups_; ++vnum) {
    IVec group_offsets;
    for (int vpos = 0; vpos < kVecNumel; ++vpos) {
      group_offsets[vpos] = metadata_.b_block_offsets_[vnum * kVecNumel + vpos];
    }
    auto start = std::chrono::high_resolution_clock::now();
    reorder(b_groups(), b_mem, b_mean(), b_sumsq(), metadata_.pt_, group_offsets);
    auto reorder_finished = std::chrono::high_resolution_clock::now();
    for (int a_group = 0; a_group < metadata_.num_a_groups_; ++a_group) {
      auto start_match = std::chrono::high_resolution_clock::now();
      matchGroup(a_groups() + a_group * kMaxBlockNumel, b_groups(), a_sumsq() + a_group * kMinBlocksInMax, b_sumsq(),
                 buf.get());
      auto end_match = std::chrono::high_resolution_clock::now();
      reduce(buf.get(), block_errors_.get() + a_group * kMinBlocksInMax * 2,
             block_matches_indices_.get() + a_group * kMinBlocksInMax * 2, b_block_nums,
             a_mean() + a_group * kMinBlocksInMax * 2, b_mean());
      auto end_reduce = std::chrono::high_resolution_clock::now();
      match_time_ += std::chrono::duration<double>(end_match - start_match);
      reduce_time_ += std::chrono::duration<double>(end_reduce - end_match);
    }
    b_block_nums += kVecNumel;
    reorder_time_ += std::chrono::duration<double>(reorder_finished - start);
  }
}
