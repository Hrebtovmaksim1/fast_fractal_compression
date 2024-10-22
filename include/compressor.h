#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "common.h"
#include "image.h"
#include "io.h"
#include "propagation.h"
#include "utils.h"

// here and everywhere "a" is the reference channel and "b" is the helper channel
class ReusableBuffers {
public:
  ReusableBuffers(int num_a_groups);

  Vec* __restrict__ a_groups() { return a_groups_.get(); }
  Vec* __restrict__ b_groups() { return b_groups_.get(); }
  Vec* __restrict__ b_mean() { return b_mean_.get(); }
  Vec* __restrict__ a_sumsq() { return a_sumsq_.get(); }
  Vec* __restrict__ b_sumsq() { return b_sumsq_.get(); }

  VecHolder<Vec> a_groups_;
  VecHolder<Vec> b_groups_;

  VecHolder<Vec> b_mean_;

  VecHolder<Vec> a_sumsq_;
  VecHolder<Vec> b_sumsq_;
};

// compresses one channel
class Compressor {
public:
  Compressor(const Channel& chl, const Metadata& metadata, ReusableBuffers& buffers);

  std::vector<float> setupCompressionState(int target_num_leafs);
  void serialize(int target_num_leafs, WStream& stream);
  void reportTimings() const;

private:
  // finds best covering of leafs for this channel
  void distributeLeafs(int target_num_leafs);

  // serializes maximum blocks' coverings
  void serializeNodes(WStream& stream, const std::vector<int>& leafs_per_block);

  // calculates best matches and associated errors for blocks of "a" and "b" channels
  void matchBlocks();

  // calculates the minimum error between all block coverings for each block for each number of leafs
  std::vector<float> propagate(int target_num_leafs);

  // serializes each block' coverings
  void serializeNode(WStream& stream, int level, int vnum, int vpos, int ipos, int num_leafs);

private:
  Vec* a_groups() { return rbuf_.a_groups_.get(); }
  Vec* b_groups() { return rbuf_.b_groups_.get(); }

  Vec* a_mean() { return a_mean_.get(); }
  Vec* b_mean() { return rbuf_.b_mean_.get(); }

  Vec* a_sumsq() { return rbuf_.a_sumsq_.get(); }
  Vec* b_sumsq() { return rbuf_.b_sumsq_.get(); }

  const Channel& a_chl_;
  ReusableBuffers& rbuf_;

  Channel b_chl_;

  const Metadata& metadata_;

  VecHolder<Vec> a_mean_;
  VecHolder<Vec> block_errors_;
  VecHolder<IVec> block_matches_indices_;

  Storage<VecHolder<Vec>> coverings_errors_;
  Storage<VecHolder<IVec>> coverings_num_leafs_in_left_;

  Propagator propagator_;

  mutable std::chrono::duration<double> reorder_time_{0};
  mutable std::chrono::duration<double> match_time_{0};
  mutable std::chrono::duration<double> reduce_time_{0};
  mutable std::chrono::duration<double> int_prop_time_{0};
  mutable std::chrono::duration<double> ext_prop_time_{0};
  mutable std::chrono::duration<double> total_setup_time_{0};
  mutable std::chrono::duration<double> serialization_time_{0};
  mutable std::chrono::duration<double> total_time_{0};
};
