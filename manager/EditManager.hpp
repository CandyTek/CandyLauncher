#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <shlwapi.h>

#include "../common/Constants.hpp"
#include "ListViewManager.hpp"
#include "EditControlPainter.hpp"
#include <gdiplus.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")

class EditManager {
public:
	// 初始化 - 设置子类过程
	static BOOL Attach(HWND hwndEdit, DWORD_PTR dwRefData) {
		//return SetWindowSubclass(hwndEdit, EditProc, 1, 0,(DWORD_PTR) dwRefData);
		InstallCaretHook(hwndEdit);
		return SetWindowSubclass(hwndEdit, EditProc, 1, dwRefData);
	}

	// 取消子类过程（可选）
	static void Detach(HWND hwndEdit) {
		UninstallCaretHook();
		RemoveWindowSubclass(hwndEdit, EditProc, 1);
	}

	static void EnableSmartEdit(HWND hEdit) {
		HRESULT hr = SHAutoComplete(hEdit, SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
		if (FAILED(hr)) {
			wchar_t buf[128];
			swprintf_s(buf, L"SHAutoComplete failed: 0x%08X", hr);
			Loge(L"EditManager", buf);
		}
	}

	// 键入字符后原生编辑框会隐藏鼠标指针，但搜索刷新列表/重绘窗口时系统会合成鼠标移动，
	// 随之而来的 WM_SETCURSOR 又把指针显示出来，看起来就是指针闪了一下。
	// 主窗口在 WM_SETCURSOR 中调用：鼠标没有真正移动时保持隐藏并返回 true
	static bool KeepCursorHiddenWhileTyping() {
		if (!s_cursorHiddenByTyping) return false;
		POINT pt{};
		if (::GetCursorPos(&pt) && pt.x == s_cursorHiddenPos.x && pt.y == s_cursorHiddenPos.y) {
			::SetCursor(nullptr);
			return true;
		}
		s_cursorHiddenByTyping = false;
		return false;
	}

private:
	inline static bool s_cursorHiddenByTyping = false;
	inline static POINT s_cursorHiddenPos{};

	// static Gdiplus::Image* g_backgroundImage = nullptr;
	static void ShowSelectedItemContextMenu(HWND hwnd) {
		if (!g_listViewHwnd || !g_pluginManager) return;
		const int index = ListView_GetNextItem(g_listViewHwnd, -1, LVNI_SELECTED);
		if (index < 0 || static_cast<size_t>(index) >= filteredActions.size()) return;

		RECT itemRect{};
		if (!ListView_GetItemRect(g_listViewHwnd, index, &itemRect, LVIR_BOUNDS)) return;
		POINT pt{itemRect.left + (itemRect.right - itemRect.left) / 2,
			itemRect.top + (itemRect.bottom - itemRect.top) / 2};
		ClientToScreen(g_listViewHwnd, &pt);

		const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		if (shiftDown ^ pref_switch_list_right_click_with_shift_right_click) {
			g_pluginManager->DispatchItemShiftRightClick(filteredActions[index], GetParent(hwnd), pt);
		} else {
			g_pluginManager->DispatchItemRightClick(filteredActions[index], GetParent(hwnd), pt);
		}
		PostMessageW(GetParent(hwnd), WM_FOCUS_EDIT, 0, 0);
	}

	static void doNumberAction(UINT vk) {
		int relativeIndex = vk - '1'; // 第几个数字键（0-8）

		// 当前可见的第一个 item
		int topIndex = ListView_GetTopIndex(g_listViewHwnd);
		// 当前一页能显示多少项
		int perPage = ListView_GetCountPerPage(g_listViewHwnd);

		// 实际对应的全局索引 = 顶部索引 + 相对索引
		int realIndex = topIndex + relativeIndex;

		if (relativeIndex < perPage &&
			realIndex < static_cast<int>(filteredActions.size())) {
			const std::shared_ptr<BaseAction> action =
				filteredActions[realIndex];
			if (action) {
				if (pref_close_after_open_item) HideWindow();
				// action->Invoke();
			}
		}
	}


	// 光标由 EditControlPainter 自绘，但系统光标不能用 HideCaret 隐藏：
	// TSF 输入法通过系统光标跟踪编辑框选区，光标处于隐藏状态时它缓存的选区不会更新，
	// 上屏全角字符后会异步发送旧的 EM_SETSEL，把光标拉回到前面的字符。
	// 因此保持系统光标可见，只把它换成全黑位图——系统按 XOR 绘制光标，黑色像素不会改变画面。
	// 原生编辑框会在多种时机（获得焦点、改字体、输入法等）重建光标，
	// 所以用 WinEvent 钩子监听光标创建，每次都立刻替换成隐形光标
	inline static HWINEVENTHOOK s_caretHook = nullptr;
	inline static HWND s_caretHookEdit = nullptr;
	inline static bool s_replacingCaret = false;

	static void InstallCaretHook(HWND hwndEdit) {
		if (s_caretHook) return;
		s_caretHookEdit = hwndEdit;
		// INCONTEXT 且只监听本线程：回调在 CreateCaret 内部同步执行，原生光标还没显示就被替换
		s_caretHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_CREATE, GetModuleHandleW(nullptr),
									CaretCreateEventProc, GetCurrentProcessId(), GetWindowThreadProcessId(hwndEdit, nullptr),
									WINEVENT_INCONTEXT);
		if (!s_caretHook) Loge(L"EditManager", L"SetWinEventHook failed: ", GetLastError());
		if (GetFocus() == hwndEdit) UseInvisibleSystemCaret(hwndEdit);
	}

	static void UninstallCaretHook() {
		if (s_caretHook) UnhookWinEvent(s_caretHook);
		s_caretHook = nullptr;
		s_caretHookEdit = nullptr;
	}

	static void CALLBACK CaretCreateEventProc(HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject, LONG, DWORD, DWORD) {
		if (idObject != OBJID_CARET || hwnd != s_caretHookEdit || s_replacingCaret) return;
				UseInvisibleSystemCaret(hwnd);
	}

	static void UseInvisibleSystemCaret(HWND hwnd) {
		static HBITMAP caretBitmap = nullptr;
		static int caretHeight = 0;

		int height = 0;
		if (const HDC hdc = GetDC(hwnd)) {
			const HGDIOBJ oldFont = SelectObject(hdc, reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0)));
			TEXTMETRICW tm{};
			if (GetTextMetricsW(hdc, &tm)) height = tm.tmHeight;
			SelectObject(hdc, oldFont);
			ReleaseDC(hwnd, hdc);
		}
		if (height <= 0) height = 16;

		POINT pt{};
		GetCaretPos(&pt);
		// 沿用被替换光标的显示状态：刚创建的原生光标是隐藏的，由编辑框随后自己 ShowCaret，
		// 这里多调用 ShowCaret 会打乱编辑框 HideCaret/ShowCaret 的配对计数
		GUITHREADINFO gti{sizeof(gti)};
		const bool wasVisible = GetGUIThreadInfo(GetWindowThreadProcessId(hwnd, nullptr), &gti) &&
			gti.hwndCaret == hwnd && (gti.flags & GUI_CARETBLINKING);
		// 新位图生效后旧光标即被销毁，此时才能释放旧位图
		HBITMAP oldBitmap = nullptr;
		if (!caretBitmap || caretHeight != height) {
			oldBitmap = caretBitmap;
			const std::vector<WORD> blackBits(static_cast<size_t>(height), 0); // 单色位图每行按 WORD 对齐
			caretBitmap = CreateBitmap(1, height, 1, 1, blackBits.data());
			caretHeight = height;
		}
		s_replacingCaret = true;
		if (caretBitmap && CreateCaret(hwnd, caretBitmap, 0, 0)) {
			SetCaretPos(pt.x, pt.y);
			if (wasVisible) ShowCaret(hwnd);
		}
		s_replacingCaret = false;
		if (oldBitmap) DeleteObject(oldBitmap);
	}

	// 影响文本/选区/滚动的可视状态，用于判断是否需要整体重绘
	struct EditVisualState {
		std::wstring text;
		DWORD selStart = 0;
		DWORD selEnd = 0;
		LRESULT firstCharPos = 0;

		bool operator==(const EditVisualState& o) const {
			return selStart == o.selStart && selEnd == o.selEnd && firstCharPos == o.firstCharPos && text == o.text;
		}
	};

	static EditVisualState CaptureVisualState(HWND hwnd) {
		EditVisualState state;
		const int length = GetWindowTextLengthW(hwnd);
		state.text.resize(length + 1);
		state.text.resize(GetWindowTextW(hwnd, state.text.data(), length + 1));
		SendMessageW(hwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&state.selStart), reinterpret_cast<LPARAM>(&state.selEnd));
		state.firstCharPos = SendMessageW(hwnd, EM_POSFROMCHAR, 0, 0);
		return state;
	}

	// 可能改变文本、选区或滚动位置的消息
	static bool MayChangeVisualState(const UINT msg) {
		switch (msg) {
		case WM_CHAR:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_IME_CHAR:
		case WM_IME_COMPOSITION:
		case WM_IME_ENDCOMPOSITION:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_MOUSEMOVE:
		case WM_SETTEXT:
		case WM_PASTE:
		case WM_CUT:
		case WM_CLEAR:
		case WM_UNDO:
		case EM_UNDO:
		case EM_REPLACESEL:
		case EM_SETSEL:
			return true;
		default:
			return false;
		}
	}

	// 子类过程函数
	// 原生编辑框改变文本后只会局部失效（IME 上屏全角字符时尤其如此），
	// BeginPaint 被裁剪到该区域，上一帧画在旧位置的自绘光标擦不掉，看起来像光标跳到了前一个字符。
	// 因此在状态发生变化后整体重绘，并让光标立即显示、重新开始闪烁计时
	static LRESULT CALLBACK EditProc(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam,
									const UINT_PTR uIdSubclass, const DWORD_PTR dwRefData) {
		if (!MayChangeVisualState(msg)) {
			return HandleEditMessage(hwnd, msg, wParam, lParam, uIdSubclass, dwRefData);
		}
		const EditVisualState before = CaptureVisualState(hwnd);
		const HCURSOR cursorBefore = ::GetCursor();
		const LRESULT result = HandleEditMessage(hwnd, msg, wParam, lParam, uIdSubclass, dwRefData);
		if ((msg == WM_CHAR || msg == WM_IME_CHAR) && cursorBefore && !::GetCursor()) {
			// 原生编辑框按“打字时隐藏指针”设置用 SetCursor(NULL) 隐藏了鼠标指针（退格不会）
			s_cursorHiddenByTyping = true;
			::GetCursorPos(&s_cursorHiddenPos);
		}
		if (IsWindow(hwnd) && !(CaptureVisualState(hwnd) == before)) {
			g_caretOn = true;
			if (GetFocus() == hwnd) ::SetTimer(hwnd, g_caretTimerId, g_caretInterval ? g_caretInterval : 530, nullptr);
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return result;
	}

	// 原生编辑框修改文本/选区时不走 WM_PAINT，而是直接 GetDC 在原始位置（不含 editbox_padding_top）画文字、擦背景，
	// 画完才发 EN_CHANGE。自绘的 WM_PAINT 优先级最低，要等 EN_CHANGE 触发的搜索结束后才执行，
	// 中间这几帧就会看到原生绘制的残影（移动光标不重画文字，所以不受影响）。
	// 不能用 WM_SETREDRAW 屏蔽原生绘制：那样原生编辑框不会横向自动滚动，也不会更新系统光标位置；
	// 也不能在 EN_CHANGE 里同步重绘：此时原生编辑框还没完成横向滚动。
	// 因此在原生处理期间用 LockWindowUpdate 锁住编辑框：锁定期间 GetDC 拿到的是空可见区域，原生绘制全部被丢弃，
	// 而编辑框内部状态（滚动、光标位置）照常更新；解锁后由 EditProc 整体失效重绘。
	// 鼠标消息不加锁：拖动选中文字可能进入 OLE 拖放的模态循环，长时间占用全局唯一的锁
	inline static bool s_nativeDrawLocked = false;

	static bool ShouldBlockNativeDrawing(const UINT msg) {
		switch (msg) {
		case WM_CHAR:
		case WM_KEYDOWN:
		case WM_IME_CHAR:
		case WM_IME_COMPOSITION:
		case WM_IME_ENDCOMPOSITION:
		case WM_SETTEXT:
		case WM_PASTE:
		case WM_CUT:
		case WM_CLEAR:
		case WM_UNDO:
		case EM_UNDO:
		case EM_REPLACESEL:
		case EM_SETSEL:
			return true;
		default:
			return false;
		}
	}

	static LRESULT DefSubclassProcWithoutNativeDrawing(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		// 嵌套调用（如 EN_CHANGE 处理中 SetWindowText）沿用外层的锁
		if (s_nativeDrawLocked || !ShouldBlockNativeDrawing(msg)) {
			return DefSubclassProc(hwnd, msg, wParam, lParam);
		}
		// 锁是系统全局唯一的，被其他窗口占用时退化为原有行为
		s_nativeDrawLocked = ::LockWindowUpdate(hwnd) != FALSE;
		const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
		if (s_nativeDrawLocked) {
			::LockWindowUpdate(nullptr);
			s_nativeDrawLocked = false;
		}
		return result;
	}

	static LRESULT HandleEditMessage(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam,
									[[maybe_unused]] UINT_PTR uIdSubclass, const DWORD_PTR dwRefData) {
		switch (msg) {
		case WM_CREATE:
			{
				g_caretInterval = ::GetCaretBlinkTime();
				if (g_caretInterval == 0 || g_caretInterval == INFINITE) g_caretInterval = 530; // 兜底
				::SetTimer(hwnd, g_caretTimerId, g_caretInterval, nullptr);
				g_caretOn = true;
				break;
			}
		case WM_COMMAND:
			{
				break;
			}
		case WM_NCPAINT:
			{
				return 0;
			}
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				PaintEdit(hwnd, hdc);
				EndPaint(hwnd, &ps);
				return 0; // 拦截默认绘制
			}

		case WM_PRINTCLIENT:
			// 父窗口要求直接在提供的 HDC 上画（比如组合绘制/双缓冲）
			PaintEdit(hwnd, (HDC)wParam);
			return 0;

		case WM_KEYDOWN:
			{
				// 1. 获取当前按下的虚拟键码
				UINT vk = static_cast<UINT>(wParam);
				if (vk == VK_APPS) {
					if (GetKeyState(VK_MENU) & 0x8000) {
						SendMessageW(hwnd, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(-1, -1));
					} else {
						ShowSelectedItemContextMenu(hwnd);
					}
					return 0;
				}
				// 2. 获取当前的修饰符状态
				UINT currentModifiers = 0;
				if (GetKeyState(VK_MENU) & 0x8000) currentModifiers |= MOD_ALT;
				if (GetKeyState(VK_CONTROL) & 0x8000) currentModifiers |= MOD_CONTROL;
				if (GetKeyState(VK_SHIFT) & 0x8000) currentModifiers |= MOD_SHIFT;
				if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) currentModifiers |= MOD_WIN_KEY;
				if (HandleListNavigationHotkey(vk, currentModifiers)) return 0;

				// 未组合修饰键
				if (currentModifiers == 0)
					switch (vk) {
					case VK_RETURN:
						SendMessage(GetParent(hwnd), WM_EDIT_CONTROL_HOTKEY, HOTKEY_ID_RUN_ITEM,
									reinterpret_cast<LPARAM>(hwnd));
						return 0;
					case VK_ESCAPE:
						{
							std::string mode = g_settings_map["pref_esc_key_feature"].stringValue;
							if (mode == "close") {
								HideWindow();
							} else if (mode == "clear") {
								SetWindowText(hwnd, L"");
								SendMessage(hwnd, EM_SETSEL, 0, -1); // 选中全部文本
								SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L""); // 替换为空
							} else if (mode == "clear_and_close") {
								int length = GetWindowTextLength(hwnd);
								if (length > 0) {
									SetWindowText(hwnd, L"");
									SendMessage(hwnd, EM_SETSEL, 0, -1); // 选中全部文本
									SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)L""); // 替换为空
								} else {
									HideWindow(); // 否则关闭
								}
							}
						}
						return 0;
					case VK_UP:
					case VK_DOWN:
						{
							NavigateListView(vk);
							return 0;
						}
					case VK_PRIOR:
					case VK_NEXT:
						{
							// 转发翻页键给listview
							SendMessage(g_listViewHwnd, msg, wParam, lParam);
							return 0;
						}

					default: break;
					}

				// 生成一个定义的快捷键值
				const UINT64 key = MAKE_HOTKEY_KEY(currentModifiers, vk);

				// 在哈希表中命中所需的快捷键
				if (const auto it = g_hotkeyMap.find(key); it != g_hotkeyMap.end()) {
					SendMessage(GetParent(hwnd), WM_EDIT_CONTROL_HOTKEY, static_cast<WPARAM>(it->second), 0);
					return 0;
				}

				if (currentModifiers == MOD_CTRL_KEY) {
					// ctrl+bs，让主界面刷新列表
					if (key == 8589934600) {
						SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(1, EN_CHANGE),
									reinterpret_cast<LPARAM>(hwnd));
					} else {
						// Ctrl + 数字键 (1-9) 快速启动列表项
						if ((pref_ctrl_number_launch_item)) {
							if (vk >= '1' && vk <= '9') {
								doNumberAction(vk);
								return 0;
							}
						}
					}
				}
				if (!filteredActions.empty()) {
					// 必须取列表中当前选中项，否则快捷键操作的目标会和界面选中项不一致
					std::shared_ptr<BaseAction>& selectedAction = GetListViewSelectedAction(
						g_listViewHwnd, filteredActions);
					int pluginResult = selectedAction
											? g_pluginManager->DispatchSendHotKey(selectedAction, vk, currentModifiers, wParam)
											: 0;
					if (pluginResult == 0) {
						break;
					} else {
						return pluginResult;
					}
				}
			}
			g_caretOn = true;
			break;
		case WM_KEYUP:
			if (wParam == VK_APPS) return 0;
			break;
		case WM_SYSCHAR:
		case WM_SYSKEYDOWN:
			{
				// alt 的快捷键监听只能在这里
				UINT vk = static_cast<UINT>(wParam);
				if (msg == WM_SYSKEYDOWN && vk == VK_APPS) {
					SendMessageW(hwnd, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(-1, -1));
					return 0;
				}
				// 2. 获取当前的修饰符状态
				UINT currentModifiers = 0;
				if (GetKeyState(VK_MENU) & 0x8000) currentModifiers |= MOD_ALT;
				if (GetKeyState(VK_CONTROL) & 0x8000) currentModifiers |= MOD_CONTROL;
				if (GetKeyState(VK_SHIFT) & 0x8000) currentModifiers |= MOD_SHIFT;
				if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) currentModifiers |= MOD_WIN_KEY;
				if (msg == WM_SYSKEYDOWN && HandleListNavigationHotkey(vk, currentModifiers)) return 0;
				if (msg == WM_SYSCHAR && vk >= 'a' && vk <= 'z') vk -= 'a' - 'A';
				if (msg == WM_SYSCHAR &&
					(MatchesListNavigationHotkey("pref_hotkey_list_up", vk, currentModifiers) ||
					 MatchesListNavigationHotkey("pref_hotkey_list_down", vk, currentModifiers))) return 0;
				// WM_SYSCHAR follows WM_SYSKEYDOWN for Alt combinations. Dispatching both
				// executes plugin shortcuts such as Alt+Enter twice.
				if (msg == WM_SYSCHAR) return 0;

				// TODO: Alt+Space: 转发给父窗口触发系统菜单，但是单独按下alt后，仍然会发出beep的声音
				// if (msg == WM_SYSKEYDOWN && vk == VK_SPACE && currentModifiers == MOD_ALT_KEY) {
				// 	PostMessage(g_mainHwnd, WM_SYSCOMMAND, SC_KEYMENU, VK_SPACE);
				// 	return 0;
				// }

				if ((pref_alt_number_launch_item && currentModifiers == MOD_ALT_KEY)) {
					if (vk >= '1' && vk <= '9') {
						doNumberAction(vk);
					}
				}
				if (!filteredActions.empty()) {
					// 必须取列表中当前选中项，否则快捷键操作的目标会和界面选中项不一致
					if (std::shared_ptr<BaseAction>& selectedAction = GetListViewSelectedAction(
						g_listViewHwnd, filteredActions)) {
						return g_pluginManager->DispatchSendHotKey(selectedAction, vk, currentModifiers, wParam);
					}
				}
				g_caretOn = true;
				return 0; // 屏蔽 ALT 键的干扰和 beep
			}
		case WM_SYSKEYUP:
			{
				return 0; // 屏蔽 ALT 键的干扰和 beep
			}
		case WM_TIMER:
			{
				if (wParam == g_caretTimerId) {
					g_caretOn = !g_caretOn;
					::InvalidateRect(hwnd, nullptr, FALSE);
					return 0;
				}
				break;
			}
		case WM_SETFOCUS:
			{
				// 获得焦点时确保定时器存在并打开
				if (g_caretInterval == 0 || g_caretInterval == INFINITE) g_caretInterval = ::GetCaretBlinkTime();
				::SetTimer(hwnd, g_caretTimerId, g_caretInterval, nullptr);
				g_caretOn = true;
				::InvalidateRect(hwnd, nullptr, FALSE);
				break;
			}
		case WM_KILLFOCUS:
			{
				// 失焦时不再显示 caret
				::KillTimer(hwnd, g_caretTimerId);
				g_caretOn = false;
				// 焦点变化时重绘，以更新hint文本显示
				InvalidateRect(hwnd, nullptr, FALSE);
				break;
			}
		case WM_NOTIFY_HEDIT_REFRESH_SKIN:
			RefreshEditPaintStyle();
			InvalidateEditPaintCache();
			return 1;
		case WM_ERASEBKGND: return 1; // 阻止默认背景擦除
		case WM_NCDESTROY:
			if (hwnd == s_caretHookEdit) UninstallCaretHook();
			RemoveWindowSubclass(hwnd, EditProc, uIdSubclass);
			break;
		case WM_DESTROY:
			{
				::KillTimer(hwnd, g_caretTimerId);
				break;
			}
		default: break;
		}
		// ctrl+o 会发出beep声，解决不了，很多原生程序的编辑框也会这样子
		return DefSubclassProcWithoutNativeDrawing(hwnd, msg, wParam, lParam);
	}
};
