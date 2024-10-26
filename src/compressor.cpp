#include "compressor.h"

#include <cassert>
#include <cmath>
#include <iostream>

#include "interface.h"
#include "metrics.h"

ReusableBuffers::ReusableBuffers(int num_a_groups) {
  a_groups_ = allocVecs(num_a_groups * kMaxBlockNumel);
  a_sumsq_ = allocVecs(num_a_groups * kMinBlocksInMax);

  b_groups_ = allocVecs(kMaxBlockNumel);
  b_mean_ = allocVecs(kMinBlocksInMax * 2);
  b_sumsq_ = allocVecs(kMinBlocksInMax);
}

Compressor::Compressor(const Channel& chl, const Metadata& metadata, ReusableBuffers& buffers)
    : a_chl_{chl}, b_chl_{a_chl_.like()}, metadata_{metadata}, rbuf_{buffers} {
  a_mean_ = allocVecs(metadata_.num_a_groups_ * kMinBlocksInMax * 2);
  block_errors_ = allocVecs(metadata_.num_a_groups_ * kMinBlocksInMax * 2);
  block_matches_indices_ = allocVecs<IVec>(metadata_.num_a_groups_ * kMinBlocksInMax * 2);

  for (int level = kMinBlockLevel; level <= kMaxBlockLevel; ++level) {
    coverings_errors_[level] = allocVecs(metadata_.num_a_groups_ * kMinBlocksInMax);
    coverings_num_leafs_in_left_[level] = allocVecs<IVec>(metadata_.num_a_groups_ * kMinBlocksInMax);
  }
}

std::vector<float> Compressor::setupCompressionState(int target_num_leafs) {
  assert(a_chl_.height() % kMaximumBlockSize.first == 0 && a_chl_.width() % kMaximumBlockSize.second == 0);

  auto start = std::chrono::high_resolution_clock::now();

  a_chl_.downsampleTo(b_chl_);
  matchBlocks();
  auto result = propagate(target_num_leafs);

  auto end = std::chrono::high_resolution_clock::now();
  total_setup_time_ = std::chrono::duration<double>(end - start);
  return result;
}

void Compressor::serialize(int target_num_leafs, WStream& stream) {
  auto start = std::chrono::high_resolution_clock::now();

  auto leafs_per_block = propagator_.distributeLeafs(target_num_leafs);
  serializeNodes(stream, leafs_per_block);

  auto end = std::chrono::high_resolution_clock::now();
  serialization_time_ = std::chrono::duration<double>(end - start);
}

void Compressor::reportTimings() const {
  std::cout << "channel compression time:" << total_setup_time_.count() + serialization_time_.count() << "\n";
  std::cout << "setup time: " << total_setup_time_.count() << "\n";
  std::cout << "reorder time: " << reorder_time_.count() << "\n";
  std::cout << "match time: " << match_time_.count() << "\n";
  std::cout << "reduce time: " << reduce_time_.count() << "\n";
  std::cout << "internal propagation time: " << int_prop_time_.count() << std::endl;
  std::cout << "external propagation time: " << ext_prop_time_.count() << std::endl;
  std::cout << "serialization time: " << serialization_time_.count() << "\n";
  std::cout << "\n";
}

void compressImage(const Image& img, const std::string& filepath, int target_size_bytes, bool report_timings) {
  auto start = std::chrono::high_resolution_clock::now();
  assertWithMessage(
      img.size().first % kMaximumBlockSize.first == 0 && img.size().second % kMaximumBlockSize.second == 0,
      "image shapes should be divisible by kMaximumBlockSize");
  assertWithMessage(
      img.size().first % kMaximumBlockSize.first == 0 && img.size().second % kMaximumBlockSize.second == 0,
      "image shapes should be divisible by kMaximumBlockSize");
  assertWithMessage(img.size().first < kMaxShape && img.size().second < kMaxShape, "image shapes are too big");

  WStream stream{};
  Metadata metadata{img.size()};
  ReusableBuffers buf{metadata.num_a_groups_};

  auto channels = img.extractChannels();

  int num_base_leafs = img.size().first * img.size().second / kMaxBlockNumel * channels.size();
  int bits_for_leaf = metadata.bits_for_match_idx_ + kBitDepth + 2;  // 2 is for "is leaf block" flags
  int target_size_bits = target_size_bytes * CHAR_BIT;
  int num_metadata_bits = kBitsPerShape * 2 + kBitsForNumChannels + channels.size() * kRangeOffset;
  // ... + num_base_leafs to account that we don't serialize a "split" flag for base blocks
  int target_num_leafs = (target_size_bits - num_metadata_bits + num_base_leafs) / bits_for_leaf;
  target_num_leafs = std::max(target_num_leafs, num_base_leafs);
  stream.dump(metadata.sz_.first, kBitsPerShape);
  stream.dump(metadata.sz_.second, kBitsPerShape);
  stream.dump(channels.size(), kBitsForNumChannels);

  std::vector<Compressor> compressors;
  std::vector<std::vector<float>> channel_errors;

  std::vector<float> weights{1};
  if (channels.size() == 3) {
    weights = {kYChannelWeight, 1, 1};
  }

  for (int channel_num = 0; channel_num < channels.size(); ++channel_num) {
    auto& chnl = channels[channel_num];

    auto range = chnl.normalize();
    stream.dump(range.first + kBitRange, kRangeOffset);
    stream.dump(range.second + kBitRange, kRangeOffset);
    compressors.emplace_back(chnl, metadata, buf);
    auto& comp = compressors.back();
    auto erorrs = comp.setupCompressionState(target_num_leafs);
    for (auto& e : erorrs) {
      e = -PSNR(e / chnl.numel()) * weights[channel_num];
    }
    channel_errors.emplace_back(std::move(erorrs));
  }

  Propagator prop{};
  prop.propagate(channel_errors, target_num_leafs);
  auto leafs_for_channel = prop.distributeLeafs(target_num_leafs - 1);
  for (int channel_num = 0; channel_num < channels.size(); ++channel_num) {
    compressors[channel_num].serialize(leafs_for_channel[channel_num], stream);
    if (report_timings) {
      std::cout << "channel " << std::to_string(channel_num) << ":\n";
      compressors[channel_num].reportTimings();
    }
  }

  stream.save(filepath);
  auto end = std::chrono::high_resolution_clock::now();
  auto total_compression_time = std::chrono::duration<double>(end - start);
  if (report_timings) {
    std::cout << "total compression time: " << total_compression_time.count() << "\n\n";
  }
}
