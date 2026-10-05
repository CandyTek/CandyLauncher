#include "QrDecoder.h"

#include <Barcode.h>
#include <HybridBinarizer.h>
#include <ImageView.h>
#include <ReaderOptions.h>
#include <qrcode/QRReader.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

std::vector<std::string> DecodeQrCodes(const uint8_t* bgra, int width, int height) {
	std::vector<uint8_t> luminance(static_cast<size_t>(width) * height);
	for (size_t i = 0; i < luminance.size(); ++i) {
		const auto* pixel = bgra + i * 4;
		luminance[i] = ZXing::RGBToLum(pixel[2], pixel[1], pixel[0]);
	}

	ZXing::ReaderOptions options;
	options.setFormats(ZXing::BarcodeFormat::QRCode);
	ZXing::QRCode::Reader reader(options);
	struct Found { std::string text; int x, y; };
	std::vector<Found> found;
	int layerWidth = width, layerHeight = height;
	int scale = 1;
	for (;;) {
		ZXing::ImageView view(luminance.data(), layerWidth, layerHeight, ZXing::ImageFormat::Lum);
		ZXing::HybridBinarizer bitmap(view);
		for (int inverted = 0; inverted < 2; ++inverted) {
			if (inverted) bitmap.invert();
			for (auto& code : reader.decode(bitmap, 0)) {
				if (!code.isValid()) continue;
				const auto& position = code.position();
				int x = (position.topLeft().x + position.bottomRight().x) * scale / 2;
				int y = (position.topLeft().y + position.bottomRight().y) * scale / 2;
				auto value = code.text();
				if (std::none_of(found.begin(), found.end(), [&](const Found& previous) {
					return previous.text == value && std::abs(previous.x - x) < 12 && std::abs(previous.y - y) < 12;
				})) found.push_back({std::move(value), x, y});
			}
		}
		if (std::max(layerWidth, layerHeight) <= 500 || std::min(layerWidth, layerHeight) < 3) break;
		const int nextWidth = layerWidth / 3, nextHeight = layerHeight / 3;
		std::vector<uint8_t> reduced(static_cast<size_t>(nextWidth) * nextHeight);
		for (int y = 0; y < nextHeight; ++y)
			for (int x = 0; x < nextWidth; ++x) {
				int sum = 4;
				for (int dy = 0; dy < 3; ++dy)
					for (int dx = 0; dx < 3; ++dx)
						sum += luminance[static_cast<size_t>(y * 3 + dy) * layerWidth + x * 3 + dx];
				reduced[static_cast<size_t>(y) * nextWidth + x] = static_cast<uint8_t>(sum / 9);
			}
		luminance = std::move(reduced);
		layerWidth = nextWidth;
		layerHeight = nextHeight;
		scale *= 3;
	}
	std::vector<std::string> results;
	for (auto& code : found) results.push_back(std::move(code.text));
	return results;
}
