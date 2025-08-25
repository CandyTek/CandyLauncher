#pragma once

#include <fstream> // Required for file operations
#include <commctrl.h> // For ListView controls
#include <gdiplus.h> // For anti-aliased drawing

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static void UpdateSwitchAnimation(SwitchInfo *switchInfo) {
	if (!switchInfo->isAnimating) return;

	DWORD currentTime = GetTickCount();
	DWORD elapsed = currentTime - switchInfo->animationStartTime;
	const DWORD ANIMATION_DURATION = 150; // 150ms animation

	if (elapsed >= ANIMATION_DURATION) {
		switchInfo->isAnimating = false;
		switchInfo->animationProgress = switchInfo->isOn ? 1.0f : 0.0f;
	} else {
		float progress = static_cast<float>(elapsed) / ANIMATION_DURATION;
		// Smooth easing function
		progress = progress * progress * (3.0f - 2.0f * progress);
		switchInfo->animationProgress = switchInfo->isOn ? progress : (1.0f - progress);
	}
}

/**
 * GDI 绘制，仿Windows8.1
 * @param hWnd 
 * @param hdc 
 * @param switchInfo 
 */
static void SwitchPaintWin8(HWND hWnd, HDC hdc, SwitchInfo* switchInfo)
{
	RECT rect;
	GetClientRect(hWnd, &rect);

	// Win8.1 风格尺寸（扁平、矩形）
	const int switchWidth  = 46;
	const int switchHeight = 18;

	// 轨道与滑块的内边距（thumb 在 track 内留白）
	const int thumbW = 12;
	const int thumbH = 20;

	// 居中
	const int switchX = (rect.right  - switchWidth)  / 2;
	const int switchY = (rect.bottom - switchHeight) / 2;
	// 背景纯白
	HBRUSH bgBrush = CreateSolidBrush(RGB(255,255,255));
	FillRect(hdc, &rect, bgBrush);
	DeleteObject(bgBrush);

	if (switchInfo) {
		UpdateSwitchAnimation(switchInfo);

		const float progress  = switchInfo->animationProgress; // 0~1
		const bool  isHovered = switchInfo->isHovered;
		const bool  isPressed = switchInfo->isPressed;

		// Win8.1 扁平色系（可按需微调）
		const COLORREF trackColorOff = isHovered ? RGB(203,203,203) : RGB(173,173,173);
		const COLORREF trackColorOn  = isHovered ? RGB(56,165,250)  : RGB(0,120,215); // 近似 Windows 蓝
		const COLORREF thumbColorOff = RGB(0,0,0);   // 关：深灰矩形
		const COLORREF thumbColorOn  = RGB(0,0,0);   // 开：白色矩形

		const COLORREF borderColorOff = RGB(173,173,173);  // 外边框：中灰
		const COLORREF borderColorOn  = borderColorOff;      // 开启时边框用强调色

		// 简单线性插值（COLORREF 是 0x00BBGGRR，按 R/G/B 拆分插值后再 RGB 合成）
		auto LerpColor = [](COLORREF a, COLORREF b, float t)->COLORREF {
			int r = (int)(GetRValue(a) + t * (GetRValue(b) - GetRValue(a)));
			int g = (int)(GetGValue(a) + t * (GetGValue(b) - GetGValue(a)));
			int bch = (int)(GetBValue(a) + t * (GetBValue(b) - GetBValue(a)));
			return RGB(r, g, bch);
		};

		const COLORREF currentTrackColor  = LerpColor(trackColorOff,  trackColorOn,  progress);
		const COLORREF currentThumbColor  = LerpColor(thumbColorOff,  thumbColorOn,  progress);
		const COLORREF currentBorderColor = LerpColor(borderColorOff, borderColorOn, progress);

		// 计算几何矩形
		RECT outerRect = { switchX, switchY, switchX + switchWidth, switchY + switchHeight }; // 外边框
		RECT trackRect = outerRect; InflateRect(&trackRect, -1, -1);                          // track 填充区（边框内缩 1px）
		RECT realTrackRect = outerRect; InflateRect(&realTrackRect, -2, -2);                          // track 填充区（边框内缩 1px）

		// thumb 位置（在 track 内左右平移）
		const int travel = (trackRect.right - trackRect.left+2)  - thumbW;      // 可移动距离
		int thumbX = trackRect.left-1  + (int)(progress * travel);
		int thumbY = trackRect.top  + ( (trackRect.bottom - trackRect.top) - thumbH ) / 2;

		// 按下时的视觉位移（扁平风格轻微按压）
		if (isPressed) { thumbX += 1; thumbY += 1; }

		RECT thumbRect = { thumbX, thumbY, thumbX + thumbW, thumbY + thumbH };

		// ---- 绘制外边框（空心矩形）----
		HPEN borderPen = CreatePen(PS_SOLID, 2, currentBorderColor);
		HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
		HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
		HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);
		Rectangle(hdc, outerRect.left, outerRect.top, outerRect.right+1, outerRect.bottom+1);

		// ---- 填充 track（纯色长矩形）----
		HBRUSH trackBrush = CreateSolidBrush(currentTrackColor);
		FillRect(hdc, &realTrackRect, trackBrush);

		// ---- 绘制 thumb（纯色小矩形 + 1px 边框）----
		HBRUSH thumbBrush = CreateSolidBrush(currentThumbColor);
		FillRect(hdc, &thumbRect, thumbBrush);

		// 清理 GDI 对象
		SelectObject(hdc, oldPen);
		SelectObject(hdc, oldBrush);
		DeleteObject(borderPen);
		DeleteObject(trackBrush);
		DeleteObject(thumbBrush);

	}

}



/**
 * GDI+ 绘制，仿WinUI switch
 * @param hWnd 
 * @param hdc 
 * @param switchInfo 
 */
static void SwitchPaintWinUi(HWND hWnd,HDC hdc,SwitchInfo* switchInfo) {
	RECT rect;
	GetClientRect(hWnd, &rect);

	// Switch dimensions (WinUI3 style)
	const int switchWidth = 44;
	const int switchHeight = 20;
	const int thumbSize = 12;

	// Center the switch in the control
	int switchX = (rect.right - switchWidth) / 2;
	int switchY = (rect.bottom - switchHeight) / 2;

	// Use double buffering
	HDC memDC = CreateCompatibleDC(hdc);
	HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
	HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

	if (switchInfo) {
		UpdateSwitchAnimation(switchInfo);

		float progress = switchInfo->animationProgress;
		bool isOn = switchInfo->isOn;
		bool isHovered = switchInfo->isHovered;
		bool isPressed = switchInfo->isPressed;

		// WinUI3 Colors - corrected
		COLORREF trackColorOff = isHovered ? RGB(200, 200, 200) : RGB(220, 220, 220);
		COLORREF trackColorOn = isHovered ? RGB(56,165,250) : RGB(0, 120, 215);
		COLORREF thumbColorOff = RGB(120, 120, 120);  // Dark gray for OFF state
		COLORREF thumbColorOn = RGB(255, 255, 255);   // White for ON state

		auto GetR = [](COLORREF c) { return GetRValue(c); };
		auto GetG = [](COLORREF c) { return GetGValue(c); };
		auto GetB = [](COLORREF c) { return GetBValue(c); };

		// Interpolate track color
		int trackR = static_cast<int>(GetR(trackColorOff) + progress * (GetR(trackColorOn) - GetR(trackColorOff)));
		int trackG = static_cast<int>(GetG(trackColorOff) + progress * (GetG(trackColorOn) - GetG(trackColorOff)));
		int trackB = static_cast<int>(GetB(trackColorOff) + progress * (GetB(trackColorOn) - GetB(trackColorOff)));

		// Interpolate thumb color
		int thumbR = static_cast<int>(GetR(thumbColorOff) + progress * (GetR(thumbColorOn) - GetR(thumbColorOff)));
		int thumbG = static_cast<int>(GetG(thumbColorOff) + progress * (GetG(thumbColorOn) - GetG(thumbColorOff)));
		int thumbB = static_cast<int>(GetB(thumbColorOff) + progress * (GetB(thumbColorOn) - GetB(thumbColorOff)));

		// Calculate thumb position with proper margins
		int thumbTravel = switchWidth - switchHeight - 4;
		int thumbX = switchX + switchHeight/2 - thumbSize/2 + 2 + static_cast<int>(progress * thumbTravel);
		int thumbY = switchY + (switchHeight - thumbSize) / 2;

		// Initialize GDI+
		Gdiplus::Graphics graphics(memDC);
		graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

		// Fill background
		graphics.Clear(Gdiplus::Color(255, 255, 255, 255));

		// Create brushes and pens
		Gdiplus::SolidBrush trackBrush(Gdiplus::Color(255, trackR, trackG, trackB));
		Gdiplus::SolidBrush thumbBrush(Gdiplus::Color(255, thumbR, thumbG, thumbB));

		// Draw track (capsule/pill shape using GraphicsPath)
		int radius = switchHeight / 2;
		Gdiplus::GraphicsPath trackPath;
		Gdiplus::RectF trackRect(static_cast<float>(switchX), static_cast<float>(switchY),
								 static_cast<float>(switchWidth), static_cast<float>(switchHeight));

		// Create capsule path: left semicircle + rectangle + right semicircle
		float cornerRadius = static_cast<float>(radius);
		trackPath.AddArc(trackRect.X, trackRect.Y, cornerRadius * 2, cornerRadius * 2, 90, 180); // Left semicircle
		trackPath.AddLine(trackRect.X + cornerRadius, trackRect.Y, trackRect.X + trackRect.Width - cornerRadius, trackRect.Y); // Top line
		trackPath.AddArc(trackRect.X + trackRect.Width - cornerRadius * 2, trackRect.Y, cornerRadius * 2, cornerRadius * 2, 270, 180); // Right semicircle
		trackPath.AddLine(trackRect.X + trackRect.Width - cornerRadius, trackRect.Y + trackRect.Height, trackRect.X + cornerRadius, trackRect.Y + trackRect.Height); // Bottom line
		trackPath.CloseFigure();

		graphics.FillPath(&trackBrush, &trackPath);

		// Draw thumb
		Gdiplus::RectF thumbRect(static_cast<float>(thumbX), static_cast<float>(thumbY),
								 static_cast<float>(thumbSize), static_cast<float>(thumbSize));

		// Draw thumb shadow for depth if pressed
		if (isPressed) {
			Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(180, 160, 160, 160));
			Gdiplus::RectF shadowRect(static_cast<float>(thumbX + 1), static_cast<float>(thumbY + 1),
									  static_cast<float>(thumbSize), static_cast<float>(thumbSize));
			graphics.FillEllipse(&shadowBrush, shadowRect);
		}

		// Draw main thumb
		graphics.FillEllipse(&thumbBrush, thumbRect);

		// Add thumb border
		Gdiplus::Pen thumbPen(Gdiplus::Color(255, 180, 180, 180), 1.0f);
		graphics.DrawEllipse(&thumbPen, thumbRect);
	} else {
		// Fill background if no switchInfo
		HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
		FillRect(memDC, &rect, bgBrush);
		DeleteObject(bgBrush);
	}

	// Copy to screen
	BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

	SelectObject(memDC, oldBitmap);
	DeleteObject(memBitmap);
	DeleteDC(memDC);

}



/**
 * GDI 绘制，矩形switch
 * @param hWnd 
 * @param hdc 
 * @param switchInfo 
 */
static void SwitchPaintRect(HWND hWnd, HDC hdc, SwitchInfo* switchInfo)
{
	RECT rect;
	GetClientRect(hWnd, &rect);

	// Win8.1 风格尺寸（扁平、矩形）
	const int switchWidth  = 44;
	const int switchHeight = 20;

	// 轨道与滑块的内边距（thumb 在 track 内留白）
	const int trackPadding = 3;
	const int thumbW = 14;
	const int thumbH = 14;

	// 居中
	const int switchX = (rect.right  - switchWidth)  / 2;
	const int switchY = (rect.bottom - switchHeight) / 2;

	// 双缓冲
	HDC memDC = CreateCompatibleDC(hdc);
	HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
	HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

	// 背景纯白
	HBRUSH bgBrush = CreateSolidBrush(RGB(255,255,255));
	FillRect(memDC, &rect, bgBrush);
	DeleteObject(bgBrush);

	if (switchInfo) {
		UpdateSwitchAnimation(switchInfo);

		const float progress  = switchInfo->animationProgress; // 0~1
		const bool  isHovered = switchInfo->isHovered;
		const bool  isPressed = switchInfo->isPressed;

		// Win8.1 扁平色系（可按需微调）
		const COLORREF trackColorOff = isHovered ? RGB(245,245,245) : RGB(240,240,240);
		const COLORREF trackColorOn  = isHovered ? RGB(0,140,235)  : RGB(0,120,215); // 近似 Windows 蓝
		const COLORREF thumbColorOff = RGB(160,160,160);   // 关：深灰矩形
		const COLORREF thumbColorOn  = RGB(255,255,255);   // 开：白色矩形

		const COLORREF borderColorOff = RGB(173,173,173);  // 外边框：中灰
		const COLORREF borderColorOn  = trackColorOn;      // 开启时边框用强调色

		// 简单线性插值（COLORREF 是 0x00BBGGRR，按 R/G/B 拆分插值后再 RGB 合成）
		auto LerpColor = [](COLORREF a, COLORREF b, float t)->COLORREF {
			int r = (int)((a & 0xFF) + t * ((b & 0xFF) - (a & 0xFF)));
			int g = (int)((((a >> 8) & 0xFF)) + t * ((((b >> 8) & 0xFF)) - (((a >> 8) & 0xFF))));
			int bch = (int)((((a >> 16) & 0xFF)) + t * ((((b >> 16) & 0xFF)) - (((a >> 16) & 0xFF))));
			return RGB(r,g,bch);
		};

		const COLORREF currentTrackColor  = LerpColor(trackColorOff,  trackColorOn,  progress);
		const COLORREF currentThumbColor  = LerpColor(thumbColorOff,  thumbColorOn,  progress);
		const COLORREF currentBorderColor = LerpColor(borderColorOff, borderColorOn, progress);

		// 计算几何矩形
		RECT outerRect = { switchX, switchY, switchX + switchWidth, switchY + switchHeight }; // 外边框
		RECT trackRect = outerRect; InflateRect(&trackRect, -1, -1);                          // track 填充区（边框内缩 1px）

		// thumb 位置（在 track 内左右平移）
		const int travel = (trackRect.right - trackRect.left) - 2*trackPadding - thumbW;      // 可移动距离
		int thumbX = trackRect.left + trackPadding + (int)(progress * travel);
		int thumbY = trackRect.top  + ( (trackRect.bottom - trackRect.top) - thumbH ) / 2;

		// 按下时的视觉位移（扁平风格轻微按压）
		if (isPressed) { thumbX += 1; thumbY += 1; }

		RECT thumbRect = { thumbX, thumbY, thumbX + thumbW, thumbY + thumbH };

		// ---- 绘制外边框（空心矩形）----
		HPEN borderPen = CreatePen(PS_SOLID, 1, currentBorderColor);
		HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
		HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
		HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, nullBrush);
		Rectangle(memDC, outerRect.left, outerRect.top, outerRect.right, outerRect.bottom);

		// ---- 填充 track（纯色长矩形）----
		HBRUSH trackBrush = CreateSolidBrush(currentTrackColor);
		FillRect(memDC, &trackRect, trackBrush);

		// ---- 绘制 thumb（纯色小矩形 + 1px 边框）----
		HBRUSH thumbBrush = CreateSolidBrush(currentThumbColor);
		FillRect(memDC, &thumbRect, thumbBrush);

		// thumb 边框（固定浅灰，保持扁平）
		HPEN thumbPen = CreatePen(PS_SOLID, 1, RGB(180,180,180));
		SelectObject(memDC, thumbPen);
		SelectObject(memDC, nullBrush);
		Rectangle(memDC, thumbRect.left, thumbRect.top, thumbRect.right, thumbRect.bottom);

		// 清理 GDI 对象
		SelectObject(memDC, oldPen);
		SelectObject(memDC, oldBrush);
		DeleteObject(borderPen);
		DeleteObject(trackBrush);
		DeleteObject(thumbBrush);
		DeleteObject(thumbPen);
	}

	// 拷贝到屏幕
	BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

	SelectObject(memDC, oldBitmap);
	DeleteObject(memBitmap);
	DeleteDC(memDC);
}


/**
 * GDI 绘制，锯齿严重
 * @param hWnd 
 * @param hdc 
 * @param switchInfo 
 */
static void SwitchPaintPoorCapsuleShape(HWND hWnd,HDC hdc,SwitchInfo* switchInfo) {
	RECT rect;
	GetClientRect(hWnd, &rect);

	// Switch dimensions (WinUI3 style)
	const int switchWidth = 44;
	const int switchHeight = 20;
	const int thumbSize = 12;

	// Center the switch in the control
	int switchX = (rect.right - switchWidth) / 2;
	int switchY = (rect.bottom - switchHeight) / 2;

	// Use double buffering
	HDC memDC = CreateCompatibleDC(hdc);
	HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
	HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

	// Fill background
	HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 255));
	FillRect(memDC, &rect, bgBrush);
	DeleteObject(bgBrush);

	if (switchInfo) {
		UpdateSwitchAnimation(switchInfo);

		float progress = switchInfo->animationProgress;
		bool isOn = switchInfo->isOn;
		bool isHovered = switchInfo->isHovered;
		bool isPressed = switchInfo->isPressed;

		// WinUI3 Colors - corrected
		COLORREF trackColorOff = isHovered ? RGB(200, 200, 200) : RGB(220, 220, 220);
		COLORREF trackColorOn = isHovered ? RGB(0, 90, 190) : RGB(0, 120, 215);
		COLORREF thumbColorOff = RGB(120, 120, 120);  // Dark gray for OFF state
		COLORREF thumbColorOn = RGB(255, 255, 255);   // White for ON state

		// Interpolate colors based on animation progress
		int trackR = static_cast<int>((trackColorOff & 0xFF) + progress * ((trackColorOn & 0xFF) - (trackColorOff & 0xFF)));
		int trackG = static_cast<int>(((trackColorOff >> 8) & 0xFF) + progress * (((trackColorOn >> 8) & 0xFF) - ((trackColorOff >> 8) & 0xFF)));
		int trackB = static_cast<int>(((trackColorOff >> 16) & 0xFF) + progress * (((trackColorOn >> 16) & 0xFF) - ((trackColorOff >> 16) & 0xFF)));
		COLORREF currentTrackColor = RGB(trackR, trackG, trackB);

		// Interpolate thumb color
		int thumbR = static_cast<int>((thumbColorOff & 0xFF) + progress * ((thumbColorOn & 0xFF) - (thumbColorOff & 0xFF)));
		int thumbG = static_cast<int>(((thumbColorOff >> 8) & 0xFF) + progress * (((thumbColorOn >> 8) & 0xFF) - ((thumbColorOff >> 8) & 0xFF)));
		int thumbB = static_cast<int>(((thumbColorOff >> 16) & 0xFF) + progress * (((thumbColorOn >> 16) & 0xFF) - ((thumbColorOff >> 16) & 0xFF)));
		COLORREF currentThumbColor = RGB(thumbR, thumbG, thumbB);

		// Enable antialiasing for smoother edges
		SetGraphicsMode(memDC, GM_ADVANCED);







		// Draw track using multiple circles for smoother appearance
		HBRUSH trackBrush = CreateSolidBrush(currentTrackColor);
		HPEN trackPen = CreatePen(PS_SOLID, 0, currentTrackColor);
		HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, trackBrush);
		HPEN oldPen = (HPEN)SelectObject(memDC, trackPen);

		// Draw track as pill shape using circles and rectangle
		int radius = switchHeight / 2;


		// Left semicircle
		Ellipse(memDC, switchX, switchY, switchX + switchHeight, switchY + switchHeight);
		// Right semicircle  
		Ellipse(memDC, switchX + switchWidth - switchHeight, switchY,
				switchX + switchWidth, switchY + switchHeight);
		// Middle rectangle
		Rectangle(memDC, switchX + radius, switchY, switchX + switchWidth - radius, switchY + switchHeight);

		// Calculate thumb position with proper margins
		int thumbTravel = switchWidth - switchHeight - 4; // Account for track radius
		int thumbX = switchX + radius - thumbSize/2 + 2 + static_cast<int>(progress * thumbTravel);
		int thumbY = switchY + (switchHeight - thumbSize) / 2;

		// Draw thumb shadow for depth
		if (isPressed) {
			HBRUSH shadowBrush = CreateSolidBrush(RGB(160, 160, 160)); // Light gray shadow
			HPEN shadowPen = CreatePen(PS_SOLID, 0, RGB(160, 160, 160));
			SelectObject(memDC, shadowBrush);
			SelectObject(memDC, shadowPen);
			Ellipse(memDC, thumbX + 1, thumbY + 1, thumbX + thumbSize + 1, thumbY + thumbSize + 1);
			DeleteObject(shadowBrush);
			DeleteObject(shadowPen);
		}

		// Draw thumb
		HBRUSH thumbBrush = CreateSolidBrush(currentThumbColor);
		HPEN thumbPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
		SelectObject(memDC, thumbBrush);
		SelectObject(memDC, thumbPen);

		Ellipse(memDC, thumbX, thumbY, thumbX + thumbSize, thumbY + thumbSize);

		// Cleanup
		SelectObject(memDC, oldBrush);
		SelectObject(memDC, oldPen);
		DeleteObject(trackBrush);
		DeleteObject(trackPen);
		DeleteObject(thumbBrush);
		DeleteObject(thumbPen);
	}

	// Copy to screen
	BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

	SelectObject(memDC, oldBitmap);
	DeleteObject(memBitmap);
	DeleteDC(memDC);

}

