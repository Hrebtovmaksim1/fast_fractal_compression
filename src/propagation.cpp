#include "propagation.h"

#include <chrono>

#include "compressor.h"

void setupInnerPropagation(const Vec* __restrict__ block_errors, Storage<VecHolder<Vec>>& coverings_errors,
                           int num_groups) {
  int offset = 0;
  for (int level = kMinBlockLevel; level <= kMaxBlockLevel; ++level) {
    Vec* __restrict__ mem = coverings_errors[level].get();
    int stride = getBlockNumel(level) / kMinBlockNumel;
    int num_blocks = kMinBlocksInMax / stride;
    for (int group = 0; group < num_groups; ++group) {
      for (int block_num = 0; block_num < num_blocks; ++block_num) {
        mem[group * kMinBlocksInMax + block_num * stride] =
            block_errors[group * kMinBlocksInMax * 2 + offset + block_num];
      }
    }
    offset += num_blocks;
  }
}

void propagateInner(Storage<VecHolder<Vec>>& coverings_errors, Storage<VecHolder<IVec>>& coverings_num_left_leafs,
                    int num_groups) {
  int offset = 0;
  for (int level = kMinBlockLevel + 1; level <= kMaxBlockLevel; ++level) {
    Vec* __restrict__ cur_erorrs_mem = coverings_errors[level].get();
    IVec* __restrict__ cur_num_left_leafs_mem = coverings_num_left_leafs[level].get();
    Vec* __restrict__ prev_mem = coverings_errors[level - 1].get();
    int cur_max_num_leafs = getBlockNumel(level) / kMinBlockNumel;
    int prev_max_num_leafs = getBlockNumel(level - 1) / kMinBlockNumel;
    int num_blocks = kMinBlocksInMax / cur_max_num_leafs;
    for (int group = 0; group < num_groups; ++group) {
      for (int block_num = 0; block_num < num_blocks; ++block_num) {
        int left_subblock_idx = block_num * 2;
        int right_subblock_idx = block_num * 2 + 1;
        int l_offset = group * kMinBlocksInMax + left_subblock_idx * prev_max_num_leafs;
        int r_offset = group * kMinBlocksInMax + right_subblock_idx * prev_max_num_leafs;

        Vec best_errors;
        IVec best_num_left_leafs;
        for (int i = 0; i < kVecNumel; ++i) {
          best_errors[i] = std::numeric_limits<float>::infinity();
        }
        for (int nl = 1; nl < cur_max_num_leafs; ++nl) {
          for (int l = std::max(nl - prev_max_num_leafs, 0); l < std::min(prev_max_num_leafs, nl); ++l) {
            int r = nl - l - 1;
            Vec lm = prev_mem[l_offset + l];
            Vec rm = prev_mem[r_offset + r];
            Vec sum = lm + rm;
            vecArgmin(best_errors, best_num_left_leafs, sum, l);
          }
          cur_erorrs_mem[group * kMinBlocksInMax + block_num * cur_max_num_leafs + nl] = best_errors;
          cur_num_left_leafs_mem[group * kMinBlocksInMax + block_num * cur_max_num_leafs + nl] = best_num_left_leafs;
        }
      }
    }
  }
}

// finds convave shell
std::vector<float> concave(const std::vector<float>& f) {
  std::vector<std::pair<float, int>> stack(f.size());
  std::vector<float> result(f.size());

  int i = 0;
  for (; f[i] == kInf; ++i) {
    result[i] = kInf;
  }
  stack[0] = std::make_pair(f[i], i);
  int ptr = 0;
  for (i += 1; i < f.size(); ++i) {
    float next_slope, prev_slope;
    auto calcSlopes = [&] {
      auto [cur_val, cur_idx] = stack[ptr];
      auto [prev_val, prev_idx] = ptr > 0 ? stack[ptr - 1] : std::make_pair(kInf, -1);
      next_slope = (cur_val - f[i]) / (i - cur_idx);
      prev_slope = (prev_val - cur_val) / (cur_idx - prev_idx);
    };

    calcSlopes();
    while (next_slope > prev_slope) [[unlikely]] {
        --ptr;
        calcSlopes();
      }
    stack[++ptr] = std::make_pair(f[i], i);
  }
  result[stack[0].second] = stack[0].first;
  for (int i = 1; i <= ptr; ++i) {
    auto [cur_val, cur_idx] = stack[i];
    auto [prev_val, prev_idx] = stack[i - 1];
    for (int rptr = prev_idx + 1; rptr < cur_idx; ++rptr) {
      result[rptr] = (prev_val * (cur_idx - rptr) + cur_val * (rptr - prev_idx)) / (cur_idx - prev_idx);
    }
    result[cur_idx] = cur_val;
  }
  result.push_back(kInf);
  return result;
}

// accelerated version of minplus convolution for almost-concave instances. O(left_instance.size() +
// right_instance.size()) if both instances are almost concave
void minPlusConvolution(const std::vector<float>& left_instance, const std::vector<float>& right_instance,
                        std::vector<float>& convolution, std::vector<int>& best_left_indices, int mx) {
  int nx = std::min<int>(mx, left_instance.size() + right_instance.size());
  convolution.resize(nx);
  best_left_indices.resize(nx);
  auto left_concave_shell = concave(left_instance);
  auto right_concave_shell = concave(right_instance);
  int lpivot = 0;
  int rpivot = 0;
  while (left_instance[lpivot] == kInf) {
    ++lpivot;
  }
  while (right_instance[rpivot] == kInf) {
    ++rpivot;
  }
  for (int i = 0; i <= lpivot + rpivot; ++i) {
    convolution[i] = kInf;
  }

  for (int k = lpivot + rpivot + 1; k < nx; ++k) {
    float best = left_instance[lpivot] + right_instance[rpivot];
    int best_i = lpivot;
    int li = lpivot - 1;
    int ri = rpivot + 1;
    for (; li >= 0 && ri < right_instance.size() && left_concave_shell[li] + right_concave_shell[ri] < best;
         --li, ++ri) {
      if (float cur = left_instance[li] + right_instance[ri]; cur < best) {
        best = cur;
        best_i = li;
      }
    }
    li = lpivot + 1;
    ri = rpivot - 1;
    for (; ri >= 0 && li < left_instance.size() && left_concave_shell[li] + right_concave_shell[ri] < best;
         ++li, --ri) {
      if (float cur = left_instance[li] + right_instance[ri]; cur < best) {
        best = cur;
        best_i = li;
      }
    }
    convolution[k] = best;
    best_left_indices[k] = best_i;

    float ldiff = left_concave_shell[lpivot] - left_concave_shell[lpivot + 1];
    float rdiff = right_concave_shell[rpivot] - right_concave_shell[rpivot + 1];
    if (ldiff > rdiff) {
      ++lpivot;
    } else {
      ++rpivot;
    }
  }
}

std::vector<float> Propagator::propagate(const CoveringsErrors& max_blocks_covering_errors, int target_num_leafs) {
  first_level_size_ = max_blocks_covering_errors.size();
  if (max_blocks_covering_errors.size() > 1) {
    auto errors = buildNextLayer(max_blocks_covering_errors, target_num_leafs);
    while (errors.size() > 1) {
      errors = buildNextLayer(errors, target_num_leafs);
    }
    return errors.back();
  } else {
    return max_blocks_covering_errors.back();
  }
}

std::vector<int> Propagator::distributeLeafs(int target_num_leafs) const {
  std::vector<int> cur{target_num_leafs};
  for (int layer = layer_num_left_leafs_.size() - 1; layer >= 0; --layer) {
    cur = distributeLeafsForPrevLevel(cur, layer);
  }
  return cur;
}

std::vector<int> Propagator::distributeLeafsForPrevLevel(const std::vector<int>& num_leafs, int layer) const {
  int prev_level_size = layer == 0 ? first_level_size_ : layer_num_left_leafs_[layer - 1].size();
  int cur_level_size = num_leafs.size();
  std::vector<int> prev_level_num_leafs{};

  for (int i = 0; i < prev_level_size / 2; ++i) {
    int l_leafs = layer_num_left_leafs_[layer][i][num_leafs[i]];
    int r_leafs = num_leafs[i] - l_leafs - 1;
    prev_level_num_leafs.push_back(l_leafs);
    prev_level_num_leafs.push_back(r_leafs);
  }
  if (prev_level_size % 2 == 1) {
    prev_level_num_leafs.push_back(num_leafs.back());
  }
  return std::move(prev_level_num_leafs);
}

CoveringsErrors Propagator::buildNextLayer(const CoveringsErrors& prev_level_coverings_errors, int target_num_leafs) {
  int new_size = (prev_level_coverings_errors.size() + 1) / 2;
  CoveringsErrors next_level_coverings_errors(new_size);
  layer_num_left_leafs_.push_back(CoveringsNumLeftLeafs(new_size));
  for (int i = 0; i < prev_level_coverings_errors.size() / 2; ++i) {
    int l = i * 2;
    int r = i * 2 + 1;
    minPlusConvolution(prev_level_coverings_errors[l], prev_level_coverings_errors[r], next_level_coverings_errors[i],
                       layer_num_left_leafs_.back()[i], target_num_leafs);
  }
  if (prev_level_coverings_errors.size() % 2 == 1) {
    next_level_coverings_errors.back() = prev_level_coverings_errors.back();
  }
  return next_level_coverings_errors;
}

std::vector<float> Compressor::propagate(int target_num_leafs) {
  auto start = std::chrono::high_resolution_clock::now();
  setupInnerPropagation(block_errors_.get(), coverings_errors_, metadata_.num_a_groups_);
  propagateInner(coverings_errors_, coverings_num_leafs_in_left_, metadata_.num_a_groups_);
  auto internal_prop_finished = std::chrono::high_resolution_clock::now();
  CoveringsErrors max_blocks_covering_errors(metadata_.num_a_blocks_);
  for (int b = 0; b < metadata_.num_a_blocks_; ++b) {
    int cg = b / kVecNumel;
    int cv = b % kVecNumel;
    int max_leafs_in_maximum_block = kMinBlocksInMax;
    max_blocks_covering_errors[b].resize(max_leafs_in_maximum_block);
    for (int nl = 0; nl < max_leafs_in_maximum_block; ++nl) {
      max_blocks_covering_errors[b][nl] =
          coverings_errors_[kMaxBlockLevel].get()[cg * max_leafs_in_maximum_block + nl][cv];
    }
  }
  auto result = propagator_.propagate(max_blocks_covering_errors, target_num_leafs);
  auto external_prop_finished = std::chrono::high_resolution_clock::now();
  auto prop_finished = std::chrono::high_resolution_clock::now();
  int_prop_time_ = std::chrono::duration<double>(internal_prop_finished - start);
  ext_prop_time_ = std::chrono::duration<double>(external_prop_finished - internal_prop_finished);

  return result;
}
