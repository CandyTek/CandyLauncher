#include "../Plugin.hpp"
#include "QrDecoder.h"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <thread>

#include "stb_image_write.h"
#include "util/MyJsonUtil.hpp"
#include "util/UrlUtil.hpp"

namespace {
struct Image {
	int width = 0, height = 0;
	int left = 0, top = 0;
	std::vector<uint8_t> pixels;
};

Image Capture(RECT rect);

struct Selection {
	RECT rect{};
	POINT start{};
	POINT end{};
	POINT origin{};
	SIZE size{};
	HDC memory = nullptr;
	HBITMAP bitmap = nullptr;
	HGDIOBJ oldBitmap = nullptr;
	uint32_t* pixels = nullptr;
	const Image* frame = nullptr;
	std::vector<uint32_t> background;
	bool dragging = false;
	bool done = false;
	bool cancelled = true;
};

RECT Normalize(POINT a, POINT b) {
	return { std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y) };
}

uint32_t PremultipliedColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
	return (uint32_t(alpha) << 24) | (uint32_t(red * alpha / 255) << 16) |
		(uint32_t(green * alpha / 255) << 8) | uint32_t(blue * alpha / 255);
}

void FillSelectionRect(Selection& state, RECT rect, uint32_t color) {
	rect.left = std::clamp(rect.left, 0L, static_cast<LONG>(state.size.cx));
	rect.right = std::clamp(rect.right, 0L, static_cast<LONG>(state.size.cx));
	rect.top = std::clamp(rect.top, 0L, static_cast<LONG>(state.size.cy));
	rect.bottom = std::clamp(rect.bottom, 0L, static_cast<LONG>(state.size.cy));
	for (LONG y = rect.top; y < rect.bottom; ++y)
		std::fill(state.pixels + static_cast<size_t>(y) * state.size.cx + rect.left,
			state.pixels + static_cast<size_t>(y) * state.size.cx + rect.right, color);
}

uint32_t TintPixel(const uint8_t* pixel, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
	const auto blend = [alpha](uint8_t original, uint8_t tint) {
		return (uint32_t(original) * (255 - alpha) + uint32_t(tint) * alpha) / 255;
	};
	return 0xff000000 | (blend(pixel[2], red) << 16) |
		(blend(pixel[1], green) << 8) | blend(pixel[0], blue);
}

void TintSelectionRect(Selection& state, RECT rect) {
	rect.left = std::clamp(rect.left, 0L, static_cast<LONG>(state.size.cx));
	rect.right = std::clamp(rect.right, 0L, static_cast<LONG>(state.size.cx));
	rect.top = std::clamp(rect.top, 0L, static_cast<LONG>(state.size.cy));
	rect.bottom = std::clamp(rect.bottom, 0L, static_cast<LONG>(state.size.cy));
	for (LONG y = rect.top; y < rect.bottom; ++y) {
		for (LONG x = rect.left; x < rect.right; ++x) {
			const size_t index = static_cast<size_t>(y) * state.size.cx + x;
			state.pixels[index] = TintPixel(state.frame->pixels.data() + index * 4, 40, 130, 245, 64);
		}
	}
}

bool RenderSelection(HWND hwnd, Selection& state) {
	const uint32_t blueFill = PremultipliedColor(40, 130, 245, 64);
	const uint32_t blueBorder = state.frame ? 0xff2d91ff : PremultipliedColor(45, 145, 255, 245);
	memcpy(state.pixels, state.background.data(), state.background.size() * sizeof(uint32_t));
	if (state.dragging) {
		RECT rect = Normalize(state.start, state.end);
		OffsetRect(&rect, -state.origin.x, -state.origin.y);
		if (state.frame) TintSelectionRect(state, rect);
		else FillSelectionRect(state, rect, blueFill);
		// The corners make the selection easier to see over a bright screenshot.
		constexpr LONG border = 2, corner = 12, cornerWidth = 4;
		FillSelectionRect(state, { rect.left, rect.top, rect.right, rect.top + border }, blueBorder);
		FillSelectionRect(state, { rect.left, rect.bottom - border, rect.right, rect.bottom }, blueBorder);
		FillSelectionRect(state, { rect.left, rect.top, rect.left + border, rect.bottom }, blueBorder);
		FillSelectionRect(state, { rect.right - border, rect.top, rect.right, rect.bottom }, blueBorder);
		if (rect.right - rect.left >= corner * 2 && rect.bottom - rect.top >= corner * 2) {
			FillSelectionRect(state, { rect.left, rect.top, rect.left + corner, rect.top + cornerWidth }, blueBorder);
			FillSelectionRect(state, { rect.left, rect.top, rect.left + cornerWidth, rect.top + corner }, blueBorder);
			FillSelectionRect(state, { rect.right - corner, rect.top, rect.right, rect.top + cornerWidth }, blueBorder);
			FillSelectionRect(state, { rect.right - cornerWidth, rect.top, rect.right, rect.top + corner }, blueBorder);
			FillSelectionRect(state, { rect.left, rect.bottom - cornerWidth, rect.left + corner, rect.bottom }, blueBorder);
			FillSelectionRect(state, { rect.left, rect.bottom - corner, rect.left + cornerWidth, rect.bottom }, blueBorder);
			FillSelectionRect(state, { rect.right - corner, rect.bottom - cornerWidth, rect.right, rect.bottom }, blueBorder);
			FillSelectionRect(state, { rect.right - cornerWidth, rect.bottom - corner, rect.right, rect.bottom }, blueBorder);
		}
	}
	POINT source{};
	BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
	return UpdateLayeredWindow(hwnd, nullptr, &state.origin, &state.size, state.memory,
		&source, 0, &blend, ULW_ALPHA) != FALSE;
}

LRESULT CALLBACK SelectProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	auto* state = reinterpret_cast<Selection*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (message) {
	case WM_NCCREATE:
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams));
		return TRUE;
	case WM_KEYDOWN:
		if (wparam == VK_ESCAPE && state) { state->done = true; DestroyWindow(hwnd); return 0; }
		break;
	case WM_LBUTTONDOWN:
		if (state) { GetCursorPos(&state->start); state->end = state->start; state->dragging = true; SetCapture(hwnd); RenderSelection(hwnd, *state); }
		return 0;
	case WM_MOUSEMOVE:
		if (state && state->dragging) { GetCursorPos(&state->end); RenderSelection(hwnd, *state); }
		return 0;
	case WM_LBUTTONUP:
		if (state && state->dragging) {
			GetCursorPos(&state->end);
			state->rect = Normalize(state->start, state->end);
			state->cancelled = state->rect.right - state->rect.left < 16 || state->rect.bottom - state->rect.top < 16;
			state->done = true;
			ReleaseCapture();
			DestroyWindow(hwnd);
		}
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_DESTROY:
		if (state) state->done = true;
		return 0;
	}
	return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool SelectArea(RECT& result, Image* frozen = nullptr) {
	// The action is invoked while the launcher is foreground; hide it before capturing.
	const HWND foreground = GetForegroundWindow();
	wchar_t windowClass[64]{};
	if (foreground && GetClassNameW(foreground, windowClass, 64) && wcscmp(windowClass, L"CandyLauncherClass") == 0)
		ShowWindow(foreground, SW_HIDE);
	const wchar_t* className = L"CandyScreenToolsSelection";
	WNDCLASSW wc{}; wc.lpfnWndProc = SelectProc; wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = className; wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
	if (!RegisterClassW(&wc)) return false;
	Selection state;
	state.origin = { GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN) };
	state.size = { GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN) };
	if (frozen) {
		Sleep(120);
		*frozen = Capture({ state.origin.x, state.origin.y,
			state.origin.x + state.size.cx, state.origin.y + state.size.cy });
		if (frozen->pixels.empty()) { UnregisterClassW(className, wc.hInstance); return false; }
		state.frame = frozen;
	}
	state.background.resize(static_cast<size_t>(state.size.cx) * state.size.cy);
	if (frozen) {
		for (size_t i = 0; i < state.background.size(); ++i)
			state.background[i] = TintPixel(frozen->pixels.data() + i * 4, 10, 18, 32, 90);
	} else {
		std::fill(state.background.begin(), state.background.end(), PremultipliedColor(10, 18, 32, 90));
	}
	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = state.size.cx;
	bmi.bmiHeader.biHeight = -state.size.cy;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	state.memory = CreateCompatibleDC(nullptr);
	if (state.memory)
		state.bitmap = CreateDIBSection(state.memory, &bmi, DIB_RGB_COLORS,
			reinterpret_cast<void**>(&state.pixels), nullptr, 0);
	if (!state.memory || !state.bitmap) {
		if (state.bitmap) DeleteObject(state.bitmap);
		if (state.memory) DeleteDC(state.memory);
		UnregisterClassW(className, wc.hInstance);
		return false;
	}
	state.oldBitmap = SelectObject(state.memory, state.bitmap);
	HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
		className, L"选择区域（Esc 取消）", WS_POPUP,
		state.origin.x, state.origin.y, state.size.cx, state.size.cy,
		nullptr, nullptr, GetModuleHandleW(nullptr), &state);
	if (!hwnd || !RenderSelection(hwnd, state)) {
		if (hwnd) DestroyWindow(hwnd);
		SelectObject(state.memory, state.oldBitmap);
		DeleteObject(state.bitmap);
		DeleteDC(state.memory);
		UnregisterClassW(className, wc.hInstance);
		return false;
	}
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);
	SetFocus(hwnd);
	MSG msg;
	int messageResult = 1;
	while (!state.done && (messageResult = GetMessageW(&msg, nullptr, 0, 0)) > 0) {
		TranslateMessage(&msg); DispatchMessageW(&msg);
	}
	if (messageResult == 0) PostQuitMessage(static_cast<int>(msg.wParam));
	if (!state.done) DestroyWindow(hwnd);
	SelectObject(state.memory, state.oldBitmap);
	DeleteObject(state.bitmap);
	DeleteDC(state.memory);
	UnregisterClassW(className, wc.hInstance);
	result = state.rect;
	return !state.cancelled;
}

Image Capture(RECT rect) {
	Image out;
	out.width = rect.right - rect.left; out.height = rect.bottom - rect.top;
	out.left = rect.left; out.top = rect.top;
	if (out.width <= 0 || out.height <= 0) return {};
	out.pixels.resize(static_cast<size_t>(out.width) * out.height * 4);
	HDC screen = GetDC(nullptr);
	HDC memory = CreateCompatibleDC(screen);
	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = out.width;
	bmi.bmiHeader.biHeight = -out.height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (!bitmap) { DeleteDC(memory); ReleaseDC(nullptr, screen); return {}; }
	HGDIOBJ old = SelectObject(memory, bitmap);
	bool ok = BitBlt(memory, 0, 0, out.width, out.height, screen, rect.left, rect.top, SRCCOPY | CAPTUREBLT) != 0;
	if (ok) memcpy(out.pixels.data(), bits, out.pixels.size());
	SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(nullptr, screen);
	return ok ? out : Image{};
}

Image CropFrozenImage(const Image& frozen, RECT rect) {
	const int left = std::clamp<LONG>(rect.left - frozen.left, 0, frozen.width);
	const int top = std::clamp<LONG>(rect.top - frozen.top, 0, frozen.height);
	const int right = std::clamp<LONG>(rect.right - frozen.left, 0, frozen.width);
	const int bottom = std::clamp<LONG>(rect.bottom - frozen.top, 0, frozen.height);
	if (right <= left || bottom <= top) return {};
	Image result;
	result.width = right - left; result.height = bottom - top;
	result.left = frozen.left + left; result.top = frozen.top + top;
	result.pixels.resize(static_cast<size_t>(result.width) * result.height * 4);
	for (int y = 0; y < result.height; ++y)
		memcpy(result.pixels.data() + static_cast<size_t>(y) * result.width * 4,
			frozen.pixels.data() + (static_cast<size_t>(top + y) * frozen.width + left) * 4,
			static_cast<size_t>(result.width) * 4);
	return result;
}

bool CopyUtf8(const std::string& text) {
	if (!OpenClipboard(nullptr)) return false;
	EmptyClipboard();
	int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (static_cast<size_t>(count) + 1) * sizeof(wchar_t));
	bool ok = false;
	if (memory) {
		auto* data = static_cast<wchar_t*>(GlobalLock(memory));
		if (data) {
			MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), data, count);
			data[count] = 0; GlobalUnlock(memory);
			ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
		}
		if (!ok) GlobalFree(memory);
	}
	CloseClipboard();
	return ok;
}

std::wstring Wide(const std::string& text) {
	int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring result(n, L'\0');
	if (n) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n);
	return result;
}

void ReadQrCode(IPluginHost* m_host) {
	RECT area{};
	Image frozen;
	if (!SelectArea(area, &frozen)) {
		// if (frozen.pixels.empty()) MessageBoxW(nullptr, L"截图失败", L"识别二维码", MB_ICONERROR);
		return;
	}
	Image image = CropFrozenImage(frozen, area);
	if (image.pixels.empty())
	{
		// MessageBoxW(nullptr, L"截图失败", L"识别二维码", MB_ICONERROR);
		return;
	}
	auto codes = DecodeQrCodes(image.pixels.data(), image.width, image.height);
	if (codes.empty())
	{
		// MessageBoxW(nullptr, L"所选区域未识别到二维码", L"识别二维码", MB_ICONINFORMATION);
		return;
	}
	std::string result;
	for (const auto& code : codes) { if (!result.empty()) result += "\r\n"; result += code; }
	m_host->ChangeEditTextText(utf8_to_wide(makeCompactJsonWithoutBraces(result)) + (isValidUrl(result) ? L"urlarg " :L"textarg "));
	m_host->MyShowWindow(SW_SHOW,true);
}

// Compare a sparse sample of the previous frame's bottom with the new frame's top.
// The best overlap is used to discard pixels already present in the output.
int FindOverlap(const Image& previous, const Image& next) {
	const int h = previous.height, w = previous.width;
	if (h != next.height || w != next.width) return 0;
	const int minOverlap = std::max(24, h / 8);
	const int maxOverlap = h - std::max(16, h / 12);
	if (minOverlap >= maxOverlap) return 0;
	int best = 0;
	double bestError = 1e9;
	for (int overlap = minOverlap; overlap <= maxOverlap; ++overlap) {
		uint64_t error = 0, samples = 0;
		for (int y = 8; y < overlap - 8; y += 9) {
			const int oldY = h - overlap + y;
			for (int x = 8; x < w - 8; x += 11) {
				size_t a = (static_cast<size_t>(oldY) * w + x) * 4;
				size_t b = (static_cast<size_t>(y) * w + x) * 4;
				for (int c = 0; c < 3; ++c) error += std::abs(int(previous.pixels[a + c]) - int(next.pixels[b + c]));
				samples += 3;
			}
		}
		if (samples && double(error) / samples < bestError) { bestError = double(error) / samples; best = overlap; }
	}
	return bestError < 10.0 ? best : 0;
}

void ScrollAt(POINT point) {
	SetCursorPos(point.x, point.y);
	INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwFlags = MOUSEEVENTF_WHEEL;
	input.mi.mouseData = static_cast<DWORD>(-WHEEL_DELTA * 3);
	SendInput(1, &input, sizeof(input));
}

void CaptureLongImage() {
	RECT area{};
	if (!SelectArea(area)) return;
	POINT center{ (area.left + area.right) / 2, (area.top + area.bottom) / 2 };
	HWND target = WindowFromPoint(center);
	if (target) SetForegroundWindow(GetAncestor(target, GA_ROOT));
	Sleep(200);
	Image first = Capture(area);
	if (first.pixels.empty()) { MessageBoxW(nullptr, L"截图失败", L"截长图", MB_ICONERROR); return; }
	Image previous = first;
	std::vector<uint8_t> output = first.pixels;
	int outputHeight = first.height;
	const int maxHeight = std::min(30000, static_cast<int>((256ull * 1024 * 1024) / (static_cast<size_t>(first.width) * 4)));
	for (int frame = 1; frame < 30 && outputHeight < maxHeight; ++frame) {
		ScrollAt(center);
		Sleep(500);
		Image next = Capture(area);
		if (next.pixels.empty()) break;
		if (next.pixels == previous.pixels) break;
		int overlap = FindOverlap(previous, next);
		if (!overlap) break;
		int added = next.height - overlap;
		if (added < 16 || outputHeight + added > maxHeight) break;
		output.insert(output.end(), next.pixels.begin() + static_cast<size_t>(overlap) * next.width * 4, next.pixels.end());
		outputHeight += added;
		previous = std::move(next);
	}
	PWSTR pictures = nullptr;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &pictures))) {
		MessageBoxW(nullptr, L"无法取得图片目录", L"截长图", MB_ICONERROR); return;
	}
	std::filesystem::path folder(pictures); CoTaskMemFree(pictures);
	SYSTEMTIME now{}; GetLocalTime(&now);
	wchar_t filename[80];
	swprintf_s(filename, L"CandyLongShot_%04d%02d%02d_%02d%02d%02d_%03d.png", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
	auto path = folder / filename;
	// Write beside the destination so the rename also works when Pictures is on another drive.
	auto temp = path; temp += L".tmp";
	// stb's file API uses the active code page on Windows, so write by callback to a wide-path FILE.
	for (size_t i = 0; i < output.size(); i += 4) {
		std::swap(output[i], output[i + 2]);
		output[i + 3] = 255;
	}
	FILE* file = nullptr;
	if (_wfopen_s(&file, temp.c_str(), L"wb") != 0 || !file) { MessageBoxW(nullptr, L"无法创建图片文件", L"截长图", MB_ICONERROR); return; }
	struct Writer { FILE* file; bool ok = true; } writer{file};
	int ok = stbi_write_png_to_func([](void* context, void* bytes, int count) {
		auto* writer = static_cast<Writer*>(context);
		if (fwrite(bytes, 1, count, writer->file) != static_cast<size_t>(count)) writer->ok = false;
	}, &writer,
		first.width, outputHeight, 4, output.data(), first.width * 4);
	if (fclose(file) != 0 || !writer.ok) ok = 0;
	if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileW(temp.c_str()); MessageBoxW(nullptr, L"保存 PNG 失败", L"截长图", MB_ICONERROR); return;
	}
	std::wstring message = L"已保存到：\n" + path.wstring();
	if (outputHeight == first.height) message += L"\n\n未检测到可拼接的滚动内容，仅保存了首屏。";
	MessageBoxW(nullptr, message.c_str(), L"截长图", MB_ICONINFORMATION);
}

class ScreenAction final : public BaseAction {
public:
	std::wstring title, subtitle, id, iconPath;
	explicit ScreenAction(uint16_t pluginIdValue) { pluginId = pluginIdValue; }
	std::wstring& getTitle() override { return title; }
	std::wstring& getSubTitle() override { return subtitle; }
	std::wstring& getIconFilePath() override { return iconPath; }
	int getIconFilePathIndex() override { return -1; }
	HBITMAP getIconBitmap() override { return nullptr; }
};
}

class ScreenToolsPlugin final : public IPlugin {
	IPluginHost* host = nullptr;
	uint16_t pluginId = 0;
	std::vector<std::shared_ptr<BaseAction>> actions;
public:
	std::wstring GetPluginName() const override { return L"ScreenToolsPlugin"; }
	std::wstring GetPluginPackageName() const override { return L"com.candytek.screentools"; }
	std::wstring GetPluginVersion() const override { return L"1.0.0"; }
	std::wstring GetPluginDescription() const override { return L"长截图与区域二维码识别"; }
	bool Initialize(IPluginHost* value) override { host = value; return host != nullptr; }
	void OnPluginIdChange(uint16_t value) override { pluginId = value; }
	void RefreshAllActions() override {
		if (!host) return;
		actions.clear();
		for (auto entry : { std::tuple{ L"long", L"截长图", L"框选滚动区域，自动滚动并保存 PNG" },
			std::tuple{ L"qr", L"框选识别二维码1", L"框选屏幕区域，识别并复制二维码内容" } }) {
			auto action = std::make_shared<ScreenAction>(pluginId);
			action->id = std::get<0>(entry); action->title = std::get<1>(entry); action->subtitle = std::get<2>(entry);
			action->matchText = host->GetTheProcessedMatchingText(action->title);
			actions.push_back(action);
		}
	}
	std::vector<std::shared_ptr<BaseAction>> GetTextMatchActions() override { return actions; }
	void Shutdown() override { actions.clear(); host = nullptr; }
	bool OnActionExecute(std::shared_ptr<BaseAction>& action, std::wstring&) override {
		auto selected = std::dynamic_pointer_cast<ScreenAction>(action);
		if (!selected) return false;
		// if (selected->id == L"long") CaptureLongImage();
		// else if (selected->id == L"qr") ReadQrCode();
		if (selected->id == L"long") {
			// 创建线程并在后台执行，通过 detach() 分离
			std::thread([this]() {
				CaptureLongImage();
			}).detach();
		}
		else if (selected->id == L"qr") {
			std::thread([this]() {
				ReadQrCode(host);
			}).detach();
		}
		else return false;
		return true;
	}
};

PLUGIN_EXPORT IPlugin* CreatePlugin() { return new ScreenToolsPlugin(); }
PLUGIN_EXPORT void DestroyPlugin(IPlugin* plugin) { delete plugin; }
PLUGIN_EXPORT int GetPluginApiVersion() { return 1; }
