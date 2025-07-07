#pragma once
#include <string>
// #include <windows.h>
// #include <vector>
// #include <gdiplus.h>

// #pragma comment(lib, "gdiplus.lib")

enum class ActionPriority
{
	Topmost = 0,
	VeryHigh = 1,
	High = 2,
	Normal = 3,
	Low = 4,
	VeryLow = 5
};

class ActionBase
{
public:
	virtual ~ActionBase() = default;
	// ActionBase() : _isExecutable(false), _hBitmap(nullptr)
	// {
		// if (gdiplusToken == 0)
			// InitGDIPlus();
	// }

	// virtual ~ActionBase()
	// {
		// if (_hBitmap)
		// {
		// 	DeleteObject(_hBitmap);
		// }
	// }

	// 图标 - 从byte数组转换为HBITMAP
	// HBITMAP GetIcon()
	// {
	// 	if (!_hBitmap && !_iconBytes.empty())
	// 	{
	// 		_hBitmap = ByteArrayToHBitmap(_iconBytes);
	// 	}
	// 	return _hBitmap;
	// }

	// void SetIconBytes(const std::vector<BYTE>& bytes)
	// {
	// 	_iconBytes = bytes;
	// 	if (_hBitmap)
	// 	{
	// 		DeleteObject(_hBitmap);
	// 		_hBitmap = nullptr;
	// 	}
	// }

	// [[nodiscard]] const std::vector<BYTE>& GetIconBytes() const
	// {
	// 	return _iconBytes;
	// }

	// 标题
	[[nodiscard]] const std::wstring& GetTitle() const { return _title; }
	void SetTitle(const std::wstring& title) { _title = title; }

	// 副标题
	[[nodiscard]] const std::wstring& GetSubtitle() const { return _subtitle; }
	void SetSubtitle(const std::wstring& subtitle) { _subtitle = subtitle; }

	// 是否可执行
	[[nodiscard]] bool IsExecutable() const { return _isExecutable; }
	void SetExecutable(const bool value) { _isExecutable = value; }

	// 抽象方法：执行动作
	virtual void Invoke() = 0;

protected:
	std::wstring _title;
	std::wstring _subtitle;
	bool _isExecutable = false;
	//static ULONG_PTR gdiplusToken; // ← 只声明，不能初始化
	// ULONG_PTR gdiplusToken = 0;

private:
	// std::vector<BYTE> _iconBytes;
	// HBITMAP _hBitmap;

	// static ULONG_PTR gdiplusToken;

	// static void InitGDIPlus()
	// {
		// Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		// Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
	// }

	// static HBITMAP ByteArrayToHBitmap(const std::vector<BYTE>& data)
	// {
	// 	IStream* pStream = nullptr;
	// 	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, data.size());
	// 	memcpy(GlobalLock(hMem), data.data(), data.size());
	// 	GlobalUnlock(hMem);
	// 	CreateStreamOnHGlobal(hMem, TRUE, &pStream);
	//
	// 	const Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(pStream);
	// 	HBITMAP hBmp = nullptr;
	// 	// bitmap->GetHBITMAP(NULL, &hBmp);
	// 	bitmap->GetHBITMAP(Gdiplus::Color(0), &hBmp);
	//
	// 	delete bitmap;
	// 	pStream->Release();
	//
	// 	return hBmp;
	// }
};

// ULONG_PTR ActionBase::gdiplusToken = 0;  // ✅ 只能出现在一个 .cpp 中
