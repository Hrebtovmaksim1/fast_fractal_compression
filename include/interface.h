#pragma once

#include <string>

#include "image.h"

void compressImage(const Image& img, const std::string& filepath, int target_size_bytes, bool report_timings = false);
Image decompressImage(const std::string& filepath, bool report_timings = false);
