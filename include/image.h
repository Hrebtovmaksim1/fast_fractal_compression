#pragma once

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "utils.h"

class Channel {
public:
  Channel(Size sz, bool fill = false) : h_{sz.first}, w_{sz.second}, buf_{std::make_unique<float[]>(h_ * w_)} {
    if (fill) {
      std::memset(buf_.get(), 0, h_ * w_ * sizeof(float));
    }
  }

  Channel(const Channel& other) = delete;
  Channel(Channel&& other) noexcept : h_{other.h_}, w_{other.w_}, buf_{std::move(other.buf_)} { }

  void downsampleTo(Channel& to, float alpha = kAlpha) const;
  std::pair<int, int> normalize();
  std::pair<float, float> getStats() const;
  void denormalize(std::pair<int, int> range);
  void save(const std::string& path) const;

  float& get(int h, int w) { return mem()[h * w_ + w]; }

  const float& get(int h, int w) const { return mem()[h * w_ + w]; }

  Channel like() const { return {Size{h_, w_}}; }

  int height() const { return h_; }
  int width() const { return w_; }
  Size size() const { return {h_, w_}; }

  int numel() const { return h_ * w_; }

  const float* mem() const { return buf_.get(); }
  float* mem() { return buf_.get(); }

private:
  int h_, w_;
  std::unique_ptr<float[]> buf_;
};

class Image {
public:
  Image(const std::string& path);
  Image(const std::vector<Channel>& channels);
  Image(Image&& other) noexcept : sz_{other.sz_}, channels_{other.channels_}, data_{std::move(other.data_)} { }

  auto numel() const { return sz_.first * sz_.second; }
  auto size() const { return sz_; }

  auto* mem() { return data_.get(); }
  const auto* mem() const { return data_.get(); }

  void save(const std::string& path);
  std::vector<Channel> extractChannels() const;

private:
  Size sz_;
  int channels_;
  std::unique_ptr<unsigned char[]> data_;
};
