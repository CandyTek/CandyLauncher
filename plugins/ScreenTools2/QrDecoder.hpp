#pragma once

#include <cstdint>
#include <string>

// Decode QR codes from a top-down BGRA image. Results are UTF-8, separated by CRLF.
std::string DecodeQrCodes(const uint8_t* pixels, int width, int height, int stride);
