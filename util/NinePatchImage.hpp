#pragma once

// 用法：
//   RECT padding;
//   auto* bmp = RenderNinePatchToSize(L"res/button.9.png", 320, 120, &padding);
//   ...使用 bmp 绘制...  最后记得 delete bmp;
//   // padding 表示内容区域相对于成品图的内边距（left/top/right/bottom）

#include <windows.h>
#include <gdiplus.h>
#include <memory>
#include <vector>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <memory>

#pragma comment(lib, "gdiplus.lib")

// using namespace Gdiplus;

struct Range {
	int a, b;
}; // [a, b] 端点都包含(用于边框黑段)
struct Segment {
	int s, e;
	bool stretch;
}; // [s, e) 半开区间(用于内容区域切片)

// -------- 基础工具 --------
static inline bool IsBlack(UINT argb) {
	BYTE a = (BYTE)(argb >> 24);
	BYTE r = (BYTE)(argb >> 16);
	BYTE g = (BYTE)(argb >> 8);
	BYTE b = (BYTE)(argb);
	// 允许任何不透明或半透明的纯黑作为标记
	return (a >= 128) && (r == 0) && (g == 0) && (b == 0);
}

static inline UINT GetARGB(Gdiplus::Bitmap* bmp, int x, int y) {
	using namespace Gdiplus;
	Color c;
	bmp->GetPixel(x, y, &c);
	return c.GetValue(); // ARGB
}

// 扫描一条边界线（1px 边），找出所有连续的黑色“段” (闭区间)
static std::vector<Range> ScanRuns(Gdiplus::Bitmap* bmp, int x0, int y0, int dx, int dy, int count) {
	std::vector<Range> runs;
	bool inRun = false;
	int runStart = -1;
	for (int i = 0; i < count; ++i) {
		int x = x0 + i * dx, y = y0 + i * dy;
		bool black = IsBlack(GetARGB(bmp, x, y));
		if (black && !inRun) {
			inRun = true;
			runStart = i;
		} else if (!black && inRun) {
			inRun = false;
			runs.push_back({runStart, i - 1});
		}
	}
	if (inRun) runs.push_back({runStart, count - 1});
	return runs;
}

// 把顶边/左边的黑段(基于外边框坐标) 转成内容区切片 (半开区间) 列表
static std::vector<Segment> BuildSegments(int contentLen, const std::vector<Range>& stretchRunsEdge) {
	// 边框坐标 i=0..contentLen-1 对应内容坐标 [0..contentLen-1]，需要整体减 1（因为内容从 (1,1) 开始）
	std::vector<Segment> segs;
	// 若未标记，则视为全可伸展
	std::vector<Range> runs = stretchRunsEdge;
	if (runs.empty()) runs.push_back({0, contentLen - 1});

	// 合法性与排序
	for (auto& r : runs) {
		r.a = std::max(0, r.a);
		r.b = std::min(contentLen - 1, r.b);
	}
	std::sort(runs.begin(), runs.end(), [](const Range& u, const Range& v) {
		return u.a < v.a;
	});

	int pos = 0;
	for (auto& r : runs) {
		int st = r.a; // 含
		int ed = r.b + 1; // 转半开
		if (st > pos) {
			segs.push_back({pos, st, false}); // 固定段
		}
		segs.push_back({st, ed, true}); // 伸展段
		pos = ed;
	}
	if (pos < contentLen) segs.push_back({pos, contentLen, false});
	return segs;
}

// 把“比例尺”分配为整数，保证总和恰好等于 targetTotal
static std::vector<int> AllocateProportional(const std::vector<int>& src, int targetTotal) {
	std::vector<int> out(src.size(), 0);
	if (src.empty()) return out;
	long long sum = std::accumulate(src.begin(), src.end(), 0LL);
	if (sum <= 0) {
		// 平分
		int base = targetTotal / (int)src.size();
		int rem = targetTotal - base * (int)src.size();
		for (size_t i = 0; i < src.size(); ++i) out[i] = base + (i < (size_t)rem ? 1 : 0);
		return out;
	}
	double acc = 0.0;
	int used = 0;
	for (size_t i = 0; i < src.size(); ++i) {
		acc += (double)src[i] * targetTotal / (double)sum;
		int v = (int)floor(acc + 1e-6) - used;
		out[i] = v;
		used += v;
	}
	// 补差
	while (used < targetTotal) {
		for (size_t i = 0; i < out.size() && used < targetTotal; ++i) {
			out[i]++;
			used++;
		}
	}
	while (used > targetTotal) {
		for (size_t i = 0; i < out.size() && used > targetTotal; ++i) {
			if (out[i] > 0) {
				out[i]--;
				used--;
			}
		}
	}
	return out;
}

// 依据“伸展/固定”列，把目标长度按规则分配到各列（或行）
static std::vector<int> LayoutTargetLengths(const std::vector<Segment>& segs, int targetLen) {
	// 拆两类
	std::vector<int> fixedIdx, stretchIdx, fixedSrc, stretchSrc;
	for (size_t i = 0; i < segs.size(); ++i) {
		int len = segs[i].e - segs[i].s;
		if (segs[i].stretch) {
			stretchIdx.push_back((int)i);
			stretchSrc.push_back(len);
		} else {
			fixedIdx.push_back((int)i);
			fixedSrc.push_back(len);
		}
	}
	int sumFixed = std::accumulate(fixedSrc.begin(), fixedSrc.end(), 0);
	int sumStretch = std::accumulate(stretchSrc.begin(), stretchSrc.end(), 0);

	std::vector<int> out(segs.size(), 0);
	if (targetLen >= sumFixed) {
		int allocStretch = targetLen - sumFixed;
		auto stretchDst = AllocateProportional(stretchSrc, allocStretch);
		// 固定段保持原始长度
		for (size_t i = 0; i < fixedIdx.size(); ++i) out[fixedIdx[i]] = fixedSrc[i];
		for (size_t i = 0; i < stretchIdx.size(); ++i) out[stretchIdx[i]] = stretchDst[i];
	} else {
		// 目标太小：按比例压缩“固定段”，伸展段置 0（常见行为）
		auto fixedDst = AllocateProportional(fixedSrc, targetLen);
		for (size_t i = 0; i < fixedIdx.size(); ++i) out[fixedIdx[i]] = fixedDst[i];
		for (size_t i = 0; i < stretchIdx.size(); ++i) out[stretchIdx[i]] = 0;
	}
	return out;
}

// 解析右/下边的 content padding（内容内边距）；若未标注，则为 {0,0,cw,ch}
static void ParseContentPadding(Gdiplus::Bitmap* bmp, int cw, int ch, RECT* outPadding) {
	if (!outPadding) return;
	int W = bmp->GetWidth(), H = bmp->GetHeight();
	// 右边 x = W-1, y=1..H-2
	auto vruns = ScanRuns(bmp, W - 1, 1, 0, 1, H - 2);
	// 下边 y = H-1, x=1..W-2
	auto hruns = ScanRuns(bmp, 1, H - 1, 1, 0, W - 2);

	int leftPad = 0, rightPad = cw, topPad = 0, bottomPad = ch;
	if (!hruns.empty()) {
		int x0 = hruns.front().a; // 边框坐标
		int x1 = hruns.back().b; // 边框坐标
		leftPad = std::max(0, x0);
		rightPad = std::max(0, cw - (x1 + 1));
	}
	if (!vruns.empty()) {
		int y0 = vruns.front().a;
		int y1 = vruns.back().b;
		topPad = std::max(0, y0);
		bottomPad = std::max(0, ch - (y1 + 1));
	}
	outPadding->left = leftPad;
	outPadding->top = topPad;
	outPadding->right = rightPad;
	outPadding->bottom = bottomPad;
}

static Gdiplus::Bitmap* GetBlankBitmap(int targetW, int targetH, RECT* outContentPadding = nullptr)
{
	Gdiplus::Bitmap* blank = new Gdiplus::Bitmap(
	targetW,
	targetH,
	PixelFormat32bppARGB
);

	Gdiplus::Graphics g(blank);
	g.Clear(Gdiplus::Color(255, 255, 255, 255)); // 透明背景

	if (outContentPadding)
	{
		outContentPadding->left = 0;
		outContentPadding->top = 0;
		outContentPadding->right = 0;
		outContentPadding->bottom = 0;
	}
	return blank;

}

// -------------------- 核心函数：读取 .9.png 并按目标尺寸绘制 --------------------
static Gdiplus::Bitmap* RenderNinePatchToSize(const wchar_t* path, int targetW, int targetH, RECT* outContentPadding = nullptr) {
	using namespace Gdiplus;
	if (!path || targetW <= 0 || targetH <= 0)
	{
		wprintf(L"RenderNinePatchToSize: invalid arguments.");
		return GetBlankBitmap(std::max(50,targetW),std::max(50,targetH),outContentPadding);
	}

	const std::unique_ptr<Bitmap> src = std::make_unique<Bitmap>(path);
	// 加载失败：打印日志并返回透明空白图
	if (!src || src->GetLastStatus() != Ok)
	{
		wprintf(L"[NinePatch] Failed to load file: %ls\n", path);
		return GetBlankBitmap(targetW,targetH,outContentPadding);
	}

	const int W = src->GetWidth();
	const int H = src->GetHeight();
	if (W < 3 || H < 3) throw std::runtime_error(".9.png too small");

	const int cw = W - 2; // 内容区域宽
	const int ch = H - 2; // 内容区域高

	// 解析伸展信息：顶边(水平) / 左边(垂直)
	auto topRuns = ScanRuns(src.get(), 1, 0, 1, 0, W - 2); // x=1..W-2, y=0
	auto leftRuns = ScanRuns(src.get(), 0, 1, 0, 1, H - 2); // y=1..H-2, x=0

	auto xSegs = BuildSegments(cw, topRuns);
	auto ySegs = BuildSegments(ch, leftRuns);

	// 内容 padding
	ParseContentPadding(src.get(), cw, ch, outContentPadding);

	// 目标位图
	auto* dst = new Bitmap(targetW, targetH, PixelFormat32bppARGB);
	Graphics g(dst);
	g.SetCompositingMode(CompositingModeSourceOver);
	g.SetCompositingQuality(CompositingQualityHighQuality);
	g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
	g.SetPixelOffsetMode(PixelOffsetModeHalf);
	g.SetSmoothingMode(SmoothingModeHighQuality);

	// 计算每列/每行在目标图的像素长度
	auto xLens = LayoutTargetLengths(xSegs, targetW);
	auto yLens = LayoutTargetLengths(ySegs, targetH);

	// 逐格拼绘
	int dy = 0;
	for (size_t r = 0; r < ySegs.size(); ++r) {
		int srcY = 1 + ySegs[r].s;
		int srcH = ySegs[r].e - ySegs[r].s;
		int dstH = yLens[r];
		if (dstH <= 0 || srcH <= 0) {
			/* 跳过空块 */
			continue;
		}

		int dx = 0;
		for (size_t c = 0; c < xSegs.size(); ++c) {
			int srcX = 1 + xSegs[c].s;
			int srcW = xSegs[c].e - xSegs[c].s;
			int dstW = xLens[c];
			if (dstW <= 0 || srcW <= 0) {
				dx += dstW;
				continue;
			}

			// 确保不绘制边界像素 - 排除最外层的引导线
			int adjustedSrcX = srcX;
			int adjustedSrcY = srcY;
			int adjustedSrcW = srcW;
			int adjustedSrcH = srcH;

			// 如果是边缘区域，需要向内收缩1像素以避免绘制引导线
			if (c == 0 && xSegs[c].s == 0) {
				adjustedSrcX++;
				adjustedSrcW--;
			} // 左边缘
			if (c == xSegs.size() - 1 && xSegs[c].e == cw) {
				adjustedSrcW--;
			} // 右边缘
			if (r == 0 && ySegs[r].s == 0) {
				adjustedSrcY++;
				adjustedSrcH--;
			} // 上边缘
			if (r == ySegs.size() - 1 && ySegs[r].e == ch) {
				adjustedSrcH--;
			} // 下边缘

			if (adjustedSrcW > 0 && adjustedSrcH > 0) {
				Rect d(dx, dy, dstW, dstH);
				g.DrawImage(src.get(), d, adjustedSrcX, adjustedSrcY, adjustedSrcW, adjustedSrcH, UnitPixel);
			}
			dx += dstW;
		}
		dy += dstH;
	}

	return dst; // 调用方负责 delete
}
