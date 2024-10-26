# Fractal Image Compression Algorithm

This repository contains an implementation of an efficient fractal compression algorithm. It achieves JPEG-like image-to-quality-ratio at high quality compression and outperforms JPEG at lower quality compression.
This is the only implementation I was able to find online to actually match jpeg image-quality-to-bpp ratio while holding somewhat reasonable performance (1MBPS for 256 * 256 image)

Of course, this is not a production code. It was only a challenge to beat other existing implementations.

## Performance

- ~0.23s for 256x256 image
- ~3.8s for 512x512 image
  
Perfmance is practically not affected by compression quality
## How It Works

The algorithm uses a DuoTree structure (different from the usual QuadTree) with block sizes ranging from 2x2 to 32x32. Each block size corresponds to a level, and each block of level N + 1 has two children blocks of level N.

### Algorithm Steps

1. **Match Finding**: Identifies the best matches for all blocks at all levels of the reference image to be compressed. Each match is a position and brightness offset of a block from the helper image.

2. **DuoTree Structure Optimization**: Iteratively determines the best DuoTree structure using a dynamic programming approach.

   Let:
   - `E(level_num, block_num, num_leafs)` be the best error for `block_num` from `level_num` if split into `num_leafs` leafs
   - `L(level_num, block_num, num_leafs)` be the corresponding number of leafs in its first children block

   The algorithm calculates `E` and `L` for each level, block, and number of leafs, then recursively backtracks to determine the optimal structure.

## Time Complexity
Both steps operate in O(N^2) time, where N is the number of pixels in the image. However, various optimizations have been implemented.

**Note**: Optimizations are tailored for a specific setup (g++ (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0, 11th Gen Intel i7-11800H (16) @ 4). Other compilers may not optimize some parts of the code as effectively.

## Building the Project
mkdir build
cd build
cmake ..
make -j
cd ..

## Running the Compression
Use the following command:
fcomp <reference_image_path> <target_size_bytes> <decompressed_image_path> <compressed_stream_path> <report_timings>

An example can be found in the `compare_with_jpeg.ipynb` notebook.

This implementation uses STB library https://github.com/nothings/stb/tree/master for loading and saving PNGs
