#include "decompressor.h"

#include "interface.h"

Decompressor::Decompressor(const Metadata& metadata) : metadata_{metadata} {
  for (int level = kMinBlockLevel; level <= kMaxBlockLevel; ++level) {
    subblock_sizes_[level] = getBlockSize(level);
  }
}

void Decompressor::apply(Channel& dst, const Channel& src) const {
  auto* dst_mem = dst.mem();
  const auto* src_mem = src.mem();
  for (const auto& tr : translations_) {
    float b_mean = 0;
    for (int h = 0; h < tr.sz_.first; ++h) {
      for (int w = 0; w < tr.sz_.second; ++w) {
        b_mean += src_mem[tr.b_mem_offset_ + h * metadata_.sz_.second + w];
      }
    }
    b_mean /= tr.sz_.first * tr.sz_.second;
    for (int h = 0; h < tr.sz_.first; ++h) {
      for (int w = 0; w < tr.sz_.second; ++w) {
        dst_mem[tr.a_mem_offset_ + h * metadata_.sz_.second + w] =
            tr.brightness_ + src_mem[tr.b_mem_offset_ + h * metadata_.sz_.second + w] - b_mean;
      }
    }
  }
}

Channel Decompressor::decompress(RStream& stream) {
  deserializeNodes(stream);
  Channel result = restore();

  return std::move(result);
}

Channel Decompressor::restore(int number_applies) const {
  auto start = std::chrono::high_resolution_clock::now();
  Channel tmp{metadata_.sz_, true};
  Channel result{metadata_.sz_};
  for (int i = 0; i < number_applies; ++i) {
    apply(result, tmp);
    result.downsampleTo(tmp);
  }
  auto end = std::chrono::high_resolution_clock::now();
  restore_time_ = std::chrono::duration<double>(end - start);
  return result;
}

void Decompressor::reportTimings() const {
  std::cout << "total channel decompression time: " << deserialization_time_.count() + restore_time_.count() << std::endl;
  std::cout << "deserialization time: " << deserialization_time_.count() << std::endl;
  std::cout << "restore time: " << restore_time_.count() << std::endl;
}

Image decompressImage(const std::string& filepath, bool report_timings) {
  auto start = std::chrono::high_resolution_clock::now();
  RStream stream{filepath};
  int h = stream.extract(kBitsPerShape);
  int w = stream.extract(kBitsPerShape);
  int nc = stream.extract(kBitsForNumChannels);

  Metadata metadata{{h, w}};
  std::vector<std::pair<int, int>> ranges;
  for (int channel_num = 0; channel_num < nc; ++channel_num) {
    std::pair<int, int> range;
    range.first = stream.extract(kRangeOffset) - kBitRange;
    range.second = stream.extract(kRangeOffset) - kBitRange;
    ranges.push_back(range);
  }
  std::vector<Channel> decompressed_channels;
  for (int channel_num = 0; channel_num < nc; ++channel_num) {
    Decompressor decomp{metadata};
    auto chnl = decomp.decompress(stream);
    chnl.denormalize(ranges[channel_num]);
    decompressed_channels.emplace_back(std::move(chnl));
    if (report_timings) {
      std::cout << "channel " << std::to_string(channel_num) << ":\n";
      decomp.reportTimings();
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto total_decompression_time = std::chrono::duration<double>(end - start);
  if (report_timings) {
    std::cout << "total decompression time: " << total_decompression_time.count() << "\n";
  }
  return Image{decompressed_channels};
}
