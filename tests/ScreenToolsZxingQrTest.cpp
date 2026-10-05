#include "../plugins/ScreenTools/QrDecoder.h"

#include <windows.h>
#include <gdiplus.h>
#include <cstdint>
#include <cstring>
#include <vector>

int wmain(int argc, wchar_t** argv) {
	if (argc != 2) return 1;
	Gdiplus::GdiplusStartupInput startup;
	ULONG_PTR token = 0;
	if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) return 2;
	int status = 0;
	{
		Gdiplus::Bitmap bitmap(argv[1]);
		if (bitmap.GetLastStatus() != Gdiplus::Ok) status = 3;
		else {
			const int width = static_cast<int>(bitmap.GetWidth());
			const int height = static_cast<int>(bitmap.GetHeight());
			Gdiplus::Rect rect(0, 0, width, height);
			Gdiplus::BitmapData data{};
			if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) != Gdiplus::Ok)
				status = 4;
			else {
				const int canvasWidth = width * 2 + 32;
				std::vector<uint8_t> pixels(static_cast<size_t>(canvasWidth) * height * 4, 255);
				for (int y = 0; y < height; ++y) {
					auto* row = pixels.data() + static_cast<size_t>(y) * canvasWidth * 4;
					const auto* source = static_cast<const uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride;
					memcpy(row, source, static_cast<size_t>(width) * 4);
					memcpy(row + static_cast<size_t>(width + 32) * 4, source, static_cast<size_t>(width) * 4);
				}
				bitmap.UnlockBits(&data);
				const auto decoded = DecodeQrCodes(pixels.data(), canvasWidth, height);
				if (decoded.size() != 2 || decoded[0] != u8"欢迎访问太平洋IT百科栏目！" || decoded[1] != decoded[0])
					status = 5;
			}
		}
	}
	Gdiplus::GdiplusShutdown(token);
	return status;
}
