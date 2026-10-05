#include "../plugins/ScreenTools2/QrDecoder.hpp"
#include <windows.h>
#include <gdiplus.h>
#include <cstdint>
#include <cstring>
#include <iostream>
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
				std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
				for (int y = 0; y < height; ++y)
					memcpy(pixels.data() + static_cast<size_t>(y) * width * 4,
						static_cast<const uint8_t*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride,
						static_cast<size_t>(width) * 4);
				bitmap.UnlockBits(&data);
				try {
					const std::string text = DecodeQrCodes(pixels.data(), width, height, width * 4);
					std::cout << "Decoded QR: " << text << '\n';
					if (text != u8"欢迎访问太平洋IT百科栏目！") status = 5;
				} catch (const std::exception& error) {
					std::cerr << error.what() << '\n';
					status = 6;
				}
			}
		}
	}
	Gdiplus::GdiplusShutdown(token);
	return status;
}
