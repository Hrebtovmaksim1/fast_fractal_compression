#pragma once

#include <climits>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class WStream {
public:
  WStream() : bits_left_{CHAR_BIT}, cur_{0}, data_{} { }

  void save(const std::string& path) {
    if (bits_left_ < CHAR_BIT) {
      data_.push_back(cur_);
    }
    std::ofstream ofs{path, std::ofstream::binary};
    ofs.write(data_.data(), data_.size());
  }

  void dump(unsigned x, int bits) {
    unsigned x_mask = 1u << (bits - 1);
    while (x_mask) {
      unsigned extracted_bit = (x & x_mask) > 0;
      cur_ += extracted_bit << (bits_left_ - 1);
      x_mask >>= 1;
      --bits_left_;
      if (bits_left_ == 0) {
        bits_left_ = CHAR_BIT;
        data_.push_back(cur_);
        cur_ = 0;
      }
    }
  }

private:
  unsigned bits_left_;
  unsigned char cur_;
  std::vector<char> data_;
};

class RStream {
public:
  RStream(const std::string& path) : bits_left_{CHAR_BIT}, cur_byte_{0}, data_{0} {
    std::ifstream ifs{path, std::ifstream::binary};
    ifs.seekg(0, std::ios::end);
    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    data_.resize(size);
    ifs.read(data_.data(), size);
  }

  unsigned extract(int bits) {
    int extracted = 0;
    while (bits) {
      unsigned extracted_bit = (data_[cur_byte_] & (1 << (bits_left_ - 1))) > 0;
      extracted += extracted_bit << (bits - 1);
      --bits_left_;
      if (bits_left_ == 0) {
        bits_left_ = CHAR_BIT;
        ++cur_byte_;
      }
      --bits;
    }
    return extracted;
  }

private:
  int bits_left_;
  int cur_byte_;
  std::vector<char> data_;
};
