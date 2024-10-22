

#include <iostream>
#include <string>

#include "interface.h"
#include "metrics.h"

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cout << "arguments: <reference_image_path>, <target_size_bytes>, <decompressed_image_path>, "
                 "<compressed_stream_path> <report_timings>\n";
  }
  std::string reference_image_path = argv[1];
  int target_size_bytes = std::atoi(argv[2]);
  std::string decompressed_image_path = argc > 3 ? argv[3] : "decompressed.png";
  std::string compressed_stream_path = argc > 4 ? argv[4] : "compressed_stream";
  bool report_timings = argc > 5 ? (std::string(argv[5]) == "true") : false;
  Image img{reference_image_path};
  compressImage(img, compressed_stream_path, target_size_bytes, report_timings);
  Image decompressed = decompressImage(compressed_stream_path, report_timings);
  decompressed.save(decompressed_image_path);
  std::cout << "PSNR: " << PSNR(img, decompressed) << std::endl;
}