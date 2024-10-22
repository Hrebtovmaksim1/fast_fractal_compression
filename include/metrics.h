#pragma once

#include "image.h"

float MSE(const Channel& c1, const Channel& c2);

float PSNR(float mse, float range = kBitRange);
float PSNR(const Channel& c1, const Channel& c2);
float PSNR(const Image& i1, const Image& i2);
