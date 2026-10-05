#include "QrDecoder.hpp"

#include <zbar.h>
#include <memory>
#include <stdexcept>
#include <vector>

std::string DecodeQrCodes(const uint8_t* pixels, int width, int height, int stride) {
	using namespace zbar;
	if (!pixels || width <= 0 || height <= 0 || stride < width * 4)
		throw std::invalid_argument("Invalid BGRA image");
	std::vector<uint8_t> gray(static_cast<size_t>(width) * height);
	for (int y = 0; y < height; ++y) {
		const uint8_t* row = pixels + static_cast<size_t>(y) * stride;
		for (int x = 0; x < width; ++x) {
			const uint8_t* pixel = row + static_cast<size_t>(x) * 4;
			gray[static_cast<size_t>(y) * width + x] =
				static_cast<uint8_t>((29 * pixel[0] + 150 * pixel[1] + 77 * pixel[2]) >> 8);
		}
	}
	std::unique_ptr<zbar_image_scanner_t, decltype(&zbar_image_scanner_destroy)> scanner(
		zbar_image_scanner_create(), zbar_image_scanner_destroy);
	std::unique_ptr<zbar_image_t, decltype(&zbar_image_destroy)> frame(
		zbar_image_create(), zbar_image_destroy);
	if (!scanner || !frame) throw std::runtime_error("Failed to initialize zbar");
	zbar_image_scanner_set_config(scanner.get(), ZBAR_NONE, ZBAR_CFG_ENABLE, 0);
	zbar_image_scanner_set_config(scanner.get(), ZBAR_QRCODE, ZBAR_CFG_ENABLE, 1);
	zbar_image_set_format(frame.get(), zbar_fourcc('Y', '8', '0', '0'));
	zbar_image_set_size(frame.get(), width, height);
	// gray belongs to std::vector; zbar must not free it when destroying the image.
	zbar_image_set_data(frame.get(), gray.data(), static_cast<unsigned long>(gray.size()),
		[](zbar_image_t*) {});
	if (zbar_scan_image(scanner.get(), frame.get()) < 0)
		throw std::runtime_error("zbar failed to scan the image");
	std::string result;
	for (const zbar_symbol_t* code = zbar_image_first_symbol(frame.get()); code; code = zbar_symbol_next(code)) {
		if (zbar_symbol_get_type(code) != ZBAR_QRCODE) continue;
		if (!result.empty()) result += "\r\n";
		result.append(zbar_symbol_get_data(code), zbar_symbol_get_data_length(code));
	}
	return result;
}
