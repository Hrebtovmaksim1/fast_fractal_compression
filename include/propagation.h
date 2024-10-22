#pragma once

#include <vector>

using CoveringsErrors = std::vector<std::vector<float>>;
using CoveringsNumLeftLeafs = std::vector<std::vector<int>>;

class Propagator {
public:
  Propagator() = default;

  std::vector<float> propagate(const CoveringsErrors& max_blocks_covering_errors, int target_num_leafs);
  std::vector<int> distributeLeafs(int target_num_leafs) const;

private:
  std::vector<int> distributeLeafsForPrevLevel(const std::vector<int>& num_leafs, int layer) const;

  CoveringsErrors buildNextLayer(const CoveringsErrors& prev_level_coverings_errors, int target_num_leafs);

  std::vector<CoveringsNumLeftLeafs> layer_num_left_leafs_;
  int first_level_size_;
};
