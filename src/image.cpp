#include "image.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

std::pair<float, float> Channel::getStats() const {
  float max = -kInf;
  float min = kInf;
  for (int i = 0; i < numel(); ++i) {
    min = std::min<float>(min, mem()[i]);
    max = std::max<float>(max, mem()[i]);
  }
  return {min, max};
}

std::pair<int, int> Channel::normalize() {
  auto stats = getStats();
  int min = stats.first;
  int max = std::max<int>(stats.second, min + 1);  // to avoid division by 0 if channel is plain
  float mul = kBitRange / float(max - min);
  for (int i = 0; i < numel(); ++i) {
    mem()[i] = (mem()[i] - min) * mul;
  }
  return {min, max};
}

void Channel::denormalize(std::pair<int, int> range) {
  auto [min, max] = range;
  float mul = kBitRange / float(max - min);
  for (int i = 0; i < numel(); ++i) {
    mem()[i] = mem()[i] / mul + min;
  }
}

void Channel::save(const std::string& path) const {
  auto tmp = new unsigned char[h_ * w_];
  for (int i = 0; i < h_ * w_; ++i) {
    tmp[i] = std::max<unsigned char>(std::min<unsigned char>(mem()[i], 255), 0);
  }
  stbi_write_png(path.c_str(), w_, h_, 1, tmp, w_);
}

static unsigned char clamp(float x) { return std::min<float>(std::max<float>(0., x), 255.); }

Image::Image(const std::string& path) {
  unsigned char* data = stbi_load(path.c_str(), &sz_.second, &sz_.first, &channels_, 0);
  //printf("Loaded image with a width of %dpx, a height of %dpx and %d channels\n", sz_.second, sz_.first, channels_);
  assertWithMessage(channels_ == 1 || channels_ == 3, "only grayscale or rgb images are supported");
  data_ = std::make_unique<unsigned char[]>(numel() * channels_);
  std::memcpy(mem(), data, numel() * channels_);
  stbi_image_free(data);
}

Image::Image(const std::vector<Channel>& channels) {
  channels_ = channels.size();
  assertWithMessage(channels_ == 1 || channels_ == 3, "only grayscale or rgb images are supported");
  sz_ = channels[0].size();
  data_ = std::make_unique<unsigned char[]>(numel() * channels_);

  if (channels_ == 1) {
    for (int i = 0; i < numel(); ++i) {
      mem()[i] = clamp(channels[0].mem()[i] + 0.5);
    }
  } else if (channels_ == 3) {
    for (int i = 0; i < numel(); ++i) {
      float y = channels[0].mem()[i];
      float u = channels[1].mem()[i];
      float v = channels[2].mem()[i];
      float r = 1 * y + 1.13988303 * v;
      float g = 1 * y - 0.39464233 * u - 0.58062185 * v;
      float b = 1 * y + 2.03206185 * u;
      mem()[i * 3] = clamp(r);
      mem()[i * 3 + 1] = clamp(g);
      mem()[i * 3 + 2] = clamp(b);
    }
  }
}

void Image::save(const std::string& path) {
  stbi_write_png(path.c_str(), sz_.second, sz_.first, channels_, mem(), sz_.second * channels_);
}

std::vector<Channel> Image::extractChannels() const {
  if (channels_ == 3) {
    std::vector<Channel> yuv;
    yuv.reserve(3);
    for (int i = 0; i < 3; ++i) {
      yuv.emplace_back(sz_);
    }
    for (int i = 0; i < numel(); ++i) {
      float r = mem()[i * 3];
      float g = mem()[i * 3 + 1];
      float b = mem()[i * 3 + 2];
      yuv[0].mem()[i] = 0.299 * r + 0.587 * g + 0.114 * b;
      yuv[1].mem()[i] = -0.14714119 * r - 0.28886916 * g + 0.43601035 * b;
      yuv[2].mem()[i] = 0.61497538 * r - 0.51496512 * g - 0.10001026 * b;
    }
    return std::move(yuv);
  } else if (channels_ == 1) {
    std::vector<Channel> ch;
    ch.emplace_back(sz_);
    for (int i = 0; i < numel(); ++i) {
      ch[0].mem()[i] = mem()[i];
    }
    return ch;
  }
  std::terminate();
}

void Channel::downsampleTo(Channel& dst, float alpha) const {
  assertWithMessage(size() == dst.size(), "channels shape mismatch");
  assertWithMessage(size().first % 2 == 0 && size().second % 2 == 0, "channel shapes should be divisible by 2");
  for (int h = 0; h < height() / 2; ++h) {
    for (int w = 0; w < width() / 2; ++w) {
      dst.get(h, w) =
          (get(h * 2, w * 2) + get(h * 2, w * 2 + 1) + get(h * 2 + 1, w * 2) + get(h * 2 + 1, w * 2 + 1)) / 4 * alpha;
    }
  }

  for (int h = 0; h < height() / 2; ++h) {
    for (int w = 0; w < width() / 2; ++w) {
      dst.get(h, w + width() / 2) = dst.get(h + height() / 2, w) = dst.get(h + height() / 2, w + width() / 2) =
          dst.get(h, w);
    }
  }
}
