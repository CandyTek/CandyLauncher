#pragma once

#include <cstdint>
#include <string>
#include <vector>

std::vector<std::string> DecodeQrCodes(const uint8_t* bgra, int width, int height);
