// 皮肤编辑器窗口
// 使用 Win32 内嵌的 WebBrowser 控件(Shell.Explorer)加载 web/skin_editor.html，
// 页面通过 window.external 调用本文件中的宿主接口读写皮肤文件；
// 拖放文件由宿主的 IDropTarget 截获，再把真实路径交给页面，便于解析皮肤里的相对图片路径

#pragma once

#include <windows.h>
#include <exdisp.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "../common/Constants.hpp"
#include "../common/GlobalState.hpp"
#include "../common/I18n.hpp"
#include "../common/Resource.h"
#include "../manager/SkinFileUtil.hpp"
#include "../util/LogUtil.hpp"
#include "../util/StringUtil.hpp"
#include "SettingsManager.hpp"

inline HWND g_skinEditorHwnd = nullptr;
// 浏览器的活动对象，主消息循环用它转发键盘消息（Tab、Delete、Ctrl+C/V 等）
inline IOleInPlaceActiveObject* g_skinEditorActiveObject = nullptr;

constexpr int SKIN_EDITOR_WIDTH = 1200;
constexpr int SKIN_EDITOR_HEIGHT = 800;

static bool SkinEditorIsZh() {
	return g_uiLanguageCode.rfind(L"zh", 0) == 0;
}

static std::wstring SkinEditorReadTextFile(const std::wstring& path, bool& ok) {
	ok = false;
	std::ifstream in(path, std::ios::binary);
	if (!in) return L"";
	std::ostringstream buffer;
	buffer << in.rdbuf();
	std::string text = buffer.str();
	if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
		static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
		text.erase(0, 3);
	}
	ok = true;
	return utf8_to_wide(text);
}

static bool SkinEditorWriteTextFile(const std::wstring& path, const std::wstring& content) {
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out) return false;
	const std::string utf8 = wide_to_utf8(content);
	out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
	return out.good();
}

static std::wstring SkinEditorFileDialog(HWND owner, bool isSave, const std::wstring& initialPath) {
	wchar_t fileBuffer[MAX_PATH * 4] = {};
	std::wstring initialDir = EXE_FOLDER_PATH + L"\\skins";
	if (!initialPath.empty()) {
		const size_t pos = initialPath.find_last_of(L"\\/");
		if (pos != std::wstring::npos) {
			initialDir = initialPath.substr(0, pos);
			wcsncpy_s(fileBuffer, initialPath.substr(pos + 1).c_str(), _TRUNCATE);
		} else {
			wcsncpy_s(fileBuffer, initialPath.c_str(), _TRUNCATE);
		}
	}

	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = owner;
	ofn.lpstrFilter = L"Skin JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = fileBuffer;
	ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuffer));
	ofn.lpstrInitialDir = initialDir.c_str();
	ofn.lpstrDefExt = L"json";
	if (isSave) {
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		if (!GetSaveFileNameW(&ofn)) return L"";
	} else {
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (!GetOpenFileNameW(&ofn)) return L"";
	}
	return fileBuffer;
}

// 返回 JSON：{"exists", "size", "modified", "fullPath"}，供属性面板显示图片文件信息
static std::wstring SkinEditorGetFileInfo(const std::wstring& path) {
	nlohmann::json info;
	wchar_t fullPath[MAX_PATH * 4] = {};
	if (!path.empty() && GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(fullPath)), fullPath, nullptr) > 0) {
		info["fullPath"] = wide_to_utf8(fullPath);
	} else {
		info["fullPath"] = wide_to_utf8(path);
	}
	WIN32_FILE_ATTRIBUTE_DATA data{};
	const bool exists = !path.empty() && GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) &&
		!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	info["exists"] = exists;
	if (exists) {
		info["size"] = (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
		FILETIME localTime{};
		SYSTEMTIME st{};
		FileTimeToLocalFileTime(&data.ftLastWriteTime, &localTime);
		FileTimeToSystemTime(&localTime, &st);
		char buffer[32] = {};
		sprintf_s(buffer, "%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		info["modified"] = buffer;
	}
	return utf8_to_wide(info.dump());
}

// 在资源管理器中定位文件；文件不存在时退而打开所在目录
static bool SkinEditorRevealFile(const std::wstring& path) {
	wchar_t fullPath[MAX_PATH * 4] = {};
	if (path.empty() || GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(fullPath)), fullPath, nullptr) == 0) {
		return false;
	}
	const DWORD attrs = GetFileAttributesW(fullPath);
	if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
		PIDLIST_ABSOLUTE pidl = nullptr;
		if (SUCCEEDED(SHParseDisplayName(fullPath, nullptr, &pidl, 0, nullptr)) && pidl) {
			const HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
			CoTaskMemFree(pidl);
			if (SUCCEEDED(hr)) return true;
		}
	}
	std::wstring folder = fullPath;
	const size_t pos = folder.find_last_of(L"\\/");
	if (pos != std::wstring::npos) folder.resize(pos);
	if (GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES) {
		Loge(L"SkinEditor", L"无法打开图片所在位置: ", path);
		return false;
	}
	return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

// 页面 window.external 上可调用的方法，注意 DISPID 从 1 开始
enum SkinEditorDispId : DISPID {
	SKIN_DISPID_IS_HOST = 1,
	SKIN_DISPID_GET_LANGUAGE,
	SKIN_DISPID_GET_CURRENT_SKIN_PATH,
	SKIN_DISPID_GET_SKIN_FOLDER,
	SKIN_DISPID_LIST_SKIN_FILES,
	SKIN_DISPID_READ_TEXT_FILE,
	SKIN_DISPID_WRITE_TEXT_FILE,
	SKIN_DISPID_OPEN_FILE_DIALOG,
	SKIN_DISPID_SAVE_FILE_DIALOG,
	SKIN_DISPID_GET_HINT_TEXT,
	SKIN_DISPID_GET_FILE_INFO,
	SKIN_DISPID_REVEAL_IN_EXPLORER,
};

static const std::pair<const wchar_t*, DISPID> SKIN_EDITOR_DISP_NAMES[] = {
	{L"isCandyHost", SKIN_DISPID_IS_HOST},
	{L"getLanguage", SKIN_DISPID_GET_LANGUAGE},
	{L"getCurrentSkinPath", SKIN_DISPID_GET_CURRENT_SKIN_PATH},
	{L"getSkinFolder", SKIN_DISPID_GET_SKIN_FOLDER},
	{L"listSkinFiles", SKIN_DISPID_LIST_SKIN_FILES},
	{L"readTextFile", SKIN_DISPID_READ_TEXT_FILE},
	{L"writeTextFile", SKIN_DISPID_WRITE_TEXT_FILE},
	{L"openFileDialog", SKIN_DISPID_OPEN_FILE_DIALOG},
	{L"saveFileDialog", SKIN_DISPID_SAVE_FILE_DIALOG},
	{L"getHintText", SKIN_DISPID_GET_HINT_TEXT},
	{L"getFileInfo", SKIN_DISPID_GET_FILE_INFO},
	{L"revealInExplorer", SKIN_DISPID_REVEAL_IN_EXPLORER},
};

// WebBrowser 控件的最小 OLE 容器实现，同时充当 window.external 与拖放目标
class SkinEditorHost final : public IOleClientSite, public IOleInPlaceSite, public IOleInPlaceFrame,
							public IDocHostUIHandler, public IDispatch, public IDropTarget {
public:
	explicit SkinEditorHost(HWND hwnd) : m_hwnd(hwnd) {}

	bool Create() {
		HRESULT hr = CoCreateInstance(CLSID_WebBrowser, nullptr, CLSCTX_INPROC_SERVER, IID_IOleObject,
									reinterpret_cast<void**>(&m_oleObject));
		if (FAILED(hr)) {
			Loge(L"SkinEditor", L"创建 WebBrowser 控件失败 hr=", hr);
			return false;
		}
		m_oleObject->SetClientSite(this);
		OleSetContainedObject(m_oleObject, TRUE);

		RECT rc;
		GetClientRect(m_hwnd, &rc);
		hr = m_oleObject->DoVerb(OLEIVERB_INPLACEACTIVATE, nullptr, this, 0, m_hwnd, &rc);
		if (FAILED(hr)) {
			Loge(L"SkinEditor", L"激活 WebBrowser 控件失败 hr=", hr);
			return false;
		}
		m_oleObject->QueryInterface(IID_IWebBrowser2, reinterpret_cast<void**>(&m_browser));
		m_oleObject->QueryInterface(IID_IOleInPlaceObject, reinterpret_cast<void**>(&m_inPlaceObject));
		m_oleObject->QueryInterface(IID_IOleInPlaceActiveObject, reinterpret_cast<void**>(&m_activeObject));
		g_skinEditorActiveObject = m_activeObject;
		return m_browser != nullptr;
	}

	void Destroy() {
		g_skinEditorActiveObject = nullptr;
		if (m_defaultDropTarget) {
			m_defaultDropTarget->Release();
			m_defaultDropTarget = nullptr;
		}
		if (m_activeObject) {
			m_activeObject->Release();
			m_activeObject = nullptr;
		}
		if (m_inPlaceObject) {
			m_inPlaceObject->InPlaceDeactivate();
			m_inPlaceObject->Release();
			m_inPlaceObject = nullptr;
		}
		if (m_browser) {
			m_browser->Release();
			m_browser = nullptr;
		}
		if (m_oleObject) {
			m_oleObject->Close(OLECLOSE_NOSAVE);
			m_oleObject->SetClientSite(nullptr);
			m_oleObject->Release();
			m_oleObject = nullptr;
		}
	}

	void Navigate(const std::wstring& url) const {
		if (!m_browser) return;
		BSTR bstrUrl = SysAllocString(url.c_str());
		VARIANT empty;
		VariantInit(&empty);
		m_browser->Navigate(bstrUrl, &empty, &empty, &empty, &empty);
		SysFreeString(bstrUrl);
	}

	void Resize() const {
		if (!m_inPlaceObject) return;
		RECT rc;
		GetClientRect(m_hwnd, &rc);
		m_inPlaceObject->SetObjectRects(&rc, &rc);
	}

	// 调用页面中的全局 JS 函数，result 可为空
	bool CallScript(const wchar_t* functionName, const std::vector<std::wstring>& args, VARIANT* result = nullptr) const {
		if (!m_browser) return false;
		IDispatch* docDispatch = nullptr;
		if (FAILED(m_browser->get_Document(&docDispatch)) || !docDispatch) return false;
		IHTMLDocument2* document = nullptr;
		docDispatch->QueryInterface(IID_IHTMLDocument2, reinterpret_cast<void**>(&document));
		docDispatch->Release();
		if (!document) return false;
		IDispatch* script = nullptr;
		document->get_Script(&script);
		document->Release();
		if (!script) return false;

		bool ok = false;
		DISPID dispId = 0;
		auto name = const_cast<LPOLESTR>(functionName);
		if (SUCCEEDED(script->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispId))) {
			// DISPPARAMS 中的参数是倒序存放的
			std::vector<VARIANT> argv(args.size());
			for (size_t i = 0; i < args.size(); ++i) {
				VARIANT& v = argv[args.size() - 1 - i];
				VariantInit(&v);
				v.vt = VT_BSTR;
				v.bstrVal = SysAllocString(args[i].c_str());
			}
			DISPPARAMS params{argv.empty() ? nullptr : argv.data(), nullptr, static_cast<UINT>(argv.size()), 0};
			ok = SUCCEEDED(script->Invoke(dispId, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result,
										nullptr, nullptr));
			for (auto& v : argv) VariantClear(&v);
		}
		script->Release();
		return ok;
	}

	// ---------- IUnknown ----------
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
		if (!ppv) return E_POINTER;
		*ppv = nullptr;
		if (riid == IID_IUnknown || riid == IID_IOleClientSite) {
			*ppv = static_cast<IOleClientSite*>(this);
		} else if (riid == IID_IOleWindow || riid == IID_IOleInPlaceSite) {
			*ppv = static_cast<IOleInPlaceSite*>(this);
		} else if (riid == IID_IOleInPlaceUIWindow || riid == IID_IOleInPlaceFrame) {
			*ppv = static_cast<IOleInPlaceFrame*>(this);
		} else if (riid == IID_IDocHostUIHandler) {
			*ppv = static_cast<IDocHostUIHandler*>(this);
		} else if (riid == IID_IDispatch) {
			*ppv = static_cast<IDispatch*>(this);
		} else if (riid == IID_IDropTarget) {
			*ppv = static_cast<IDropTarget*>(this);
		} else {
			return E_NOINTERFACE;
		}
		AddRef();
		return S_OK;
	}

	STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }

	STDMETHODIMP_(ULONG) Release() override {
		const ULONG count = InterlockedDecrement(&m_refCount);
		if (count == 0) delete this;
		return count;
	}

	// ---------- IOleClientSite ----------
	STDMETHODIMP SaveObject() override { return E_NOTIMPL; }
	STDMETHODIMP GetMoniker(DWORD, DWORD, IMoniker**) override { return E_NOTIMPL; }
	STDMETHODIMP GetContainer(IOleContainer** ppContainer) override {
		*ppContainer = nullptr;
		return E_NOINTERFACE;
	}
	STDMETHODIMP ShowObject() override { return S_OK; }
	STDMETHODIMP OnShowWindow(BOOL) override { return S_OK; }
	STDMETHODIMP RequestNewObjectLayout() override { return E_NOTIMPL; }

	// ---------- IOleWindow ----------
	STDMETHODIMP GetWindow(HWND* phwnd) override {
		*phwnd = m_hwnd;
		return S_OK;
	}
	STDMETHODIMP ContextSensitiveHelp(BOOL) override { return E_NOTIMPL; }

	// ---------- IOleInPlaceSite ----------
	STDMETHODIMP CanInPlaceActivate() override { return S_OK; }
	STDMETHODIMP OnInPlaceActivate() override { return S_OK; }
	STDMETHODIMP OnUIActivate() override { return S_OK; }
	STDMETHODIMP GetWindowContext(IOleInPlaceFrame** ppFrame, IOleInPlaceUIWindow** ppDoc, LPRECT lprcPosRect,
								LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo) override {
		*ppFrame = static_cast<IOleInPlaceFrame*>(this);
		AddRef();
		*ppDoc = nullptr;
		GetClientRect(m_hwnd, lprcPosRect);
		GetClientRect(m_hwnd, lprcClipRect);
		lpFrameInfo->fMDIApp = FALSE;
		lpFrameInfo->hwndFrame = m_hwnd;
		lpFrameInfo->haccel = nullptr;
		lpFrameInfo->cAccelEntries = 0;
		return S_OK;
	}
	STDMETHODIMP Scroll(SIZE) override { return E_NOTIMPL; }
	STDMETHODIMP OnUIDeactivate(BOOL) override { return S_OK; }
	STDMETHODIMP OnInPlaceDeactivate() override { return S_OK; }
	STDMETHODIMP DiscardUndoState() override { return E_NOTIMPL; }
	STDMETHODIMP DeactivateAndUndo() override { return E_NOTIMPL; }
	STDMETHODIMP OnPosRectChange(LPCRECT lprcPosRect) override {
		if (m_inPlaceObject) m_inPlaceObject->SetObjectRects(lprcPosRect, lprcPosRect);
		return S_OK;
	}

	// ---------- IOleInPlaceUIWindow / IOleInPlaceFrame ----------
	STDMETHODIMP GetBorder(LPRECT) override { return E_NOTIMPL; }
	STDMETHODIMP RequestBorderSpace(LPCBORDERWIDTHS) override { return E_NOTIMPL; }
	STDMETHODIMP SetBorderSpace(LPCBORDERWIDTHS) override { return E_NOTIMPL; }
	STDMETHODIMP SetActiveObject(IOleInPlaceActiveObject*, LPCOLESTR) override { return S_OK; }
	STDMETHODIMP InsertMenus(HMENU, LPOLEMENUGROUPWIDTHS) override { return E_NOTIMPL; }
	STDMETHODIMP SetMenu(HMENU, HOLEMENU, HWND) override { return S_OK; }
	STDMETHODIMP RemoveMenus(HMENU) override { return E_NOTIMPL; }
	STDMETHODIMP SetStatusText(LPCOLESTR) override { return S_OK; }
	// IOleInPlaceFrame 与 IDocHostUIHandler 共用同一签名的 EnableModeless
	STDMETHODIMP EnableModeless(BOOL) override { return S_OK; }
	STDMETHODIMP TranslateAccelerator(LPMSG, WORD) override { return S_FALSE; }

	// ---------- IDocHostUIHandler ----------
	STDMETHODIMP ShowContextMenu(DWORD dwID, POINT*, IUnknown*, IDispatch*) override {
		// 只保留编辑框和选中文本的右键菜单（复制粘贴），屏蔽带“刷新”的页面默认菜单
		return (dwID == CONTEXT_MENU_CONTROL || dwID == CONTEXT_MENU_TEXTSELECT) ? S_FALSE : S_OK;
	}
	STDMETHODIMP GetHostInfo(DOCHOSTUIINFO* pInfo) override {
		pInfo->cbSize = sizeof(DOCHOSTUIINFO);
		pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_THEME | DOCHOSTUIFLAG_DPI_AWARE;
		pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
		return S_OK;
	}
	STDMETHODIMP ShowUI(DWORD, IOleInPlaceActiveObject*, IOleCommandTarget*, IOleInPlaceFrame*,
						IOleInPlaceUIWindow*) override { return S_OK; }
	STDMETHODIMP HideUI() override { return S_OK; }
	STDMETHODIMP UpdateUI() override { return S_OK; }
	STDMETHODIMP OnDocWindowActivate(BOOL) override { return S_OK; }
	STDMETHODIMP OnFrameWindowActivate(BOOL) override { return S_OK; }
	STDMETHODIMP ResizeBorder(LPCRECT, IOleInPlaceUIWindow*, BOOL) override { return S_OK; }
	STDMETHODIMP TranslateAccelerator(LPMSG lpMsg, const GUID*, DWORD) override {
		// 屏蔽刷新、新建窗口、打印等会丢失编辑内容或无意义的浏览器快捷键
		if (lpMsg->message == WM_KEYDOWN) {
			const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
			if (lpMsg->wParam == VK_F5 || (ctrl && (lpMsg->wParam == 'R' || lpMsg->wParam == 'N' ||
				lpMsg->wParam == 'P' || lpMsg->wParam == 'L'))) {
				return S_OK;
			}
		}
		return S_FALSE;
	}
	STDMETHODIMP GetOptionKeyPath(LPOLESTR* pchKey, DWORD) override {
		*pchKey = nullptr;
		return E_NOTIMPL;
	}
	STDMETHODIMP GetDropTarget(IDropTarget* pDropTarget, IDropTarget** ppDropTarget) override {
		if (m_defaultDropTarget) m_defaultDropTarget->Release();
		m_defaultDropTarget = pDropTarget;
		if (m_defaultDropTarget) m_defaultDropTarget->AddRef();
		*ppDropTarget = static_cast<IDropTarget*>(this);
		AddRef();
		return S_OK;
	}
	STDMETHODIMP GetExternal(IDispatch** ppDispatch) override {
		*ppDispatch = static_cast<IDispatch*>(this);
		AddRef();
		return S_OK;
	}
	STDMETHODIMP TranslateUrl(DWORD, LPWSTR, LPWSTR* ppchURLOut) override {
		*ppchURLOut = nullptr;
		return S_FALSE;
	}
	STDMETHODIMP FilterDataObject(IDataObject*, IDataObject** ppDORet) override {
		*ppDORet = nullptr;
		return S_FALSE;
	}

	// ---------- IDispatch (window.external) ----------
	STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) override {
		*pctinfo = 0;
		return S_OK;
	}
	STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
	STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR* rgszNames, UINT cNames, LCID, DISPID* rgDispId) override {
		HRESULT hr = S_OK;
		for (UINT i = 0; i < cNames; ++i) {
			rgDispId[i] = DISPID_UNKNOWN;
			for (const auto& [name, id] : SKIN_EDITOR_DISP_NAMES) {
				if (_wcsicmp(rgszNames[i], name) == 0) {
					rgDispId[i] = id;
					break;
				}
			}
			if (rgDispId[i] == DISPID_UNKNOWN) hr = DISP_E_UNKNOWNNAME;
		}
		return hr;
	}
	STDMETHODIMP Invoke(DISPID dispIdMember, REFIID, LCID, WORD wFlags, DISPPARAMS* pDispParams,
						VARIANT* pVarResult, EXCEPINFO*, UINT*) override {
		if (!(wFlags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
		switch (dispIdMember) {
		case SKIN_DISPID_IS_HOST:
			SetBoolResult(pVarResult, true);
			return S_OK;
		case SKIN_DISPID_GET_LANGUAGE:
			SetStringResult(pVarResult, SkinEditorIsZh() ? L"zh" : L"en");
			return S_OK;
		case SKIN_DISPID_GET_CURRENT_SKIN_PATH:
			SetStringResult(pVarResult, getCurrectSkinPath(g_currectSkinFilePath));
			return S_OK;
		case SKIN_DISPID_GET_SKIN_FOLDER:
			SetStringResult(pVarResult, EXE_FOLDER_PATH + L"\\skins");
			return S_OK;
		case SKIN_DISPID_LIST_SKIN_FILES:
			{
				// 换行分隔的完整路径列表
				std::wstring joined;
				for (const auto& path : FindSkinFiles()) {
					joined += getCurrectSkinPath(path) + L"\n";
				}
				SetStringResult(pVarResult, joined);
				return S_OK;
			}
		case SKIN_DISPID_READ_TEXT_FILE:
			{
				bool ok = false;
				const std::wstring text = SkinEditorReadTextFile(GetStringArg(pDispParams, 0), ok);
				if (ok) {
					SetStringResult(pVarResult, text);
				} else if (pVarResult) {
					VariantInit(pVarResult);
					pVarResult->vt = VT_NULL;
				}
				return S_OK;
			}
		case SKIN_DISPID_WRITE_TEXT_FILE:
			{
				const std::wstring path = GetStringArg(pDispParams, 0);
				const bool ok = !path.empty() && SkinEditorWriteTextFile(path, GetStringArg(pDispParams, 1));
				if (!ok) Loge(L"SkinEditor", L"保存皮肤文件失败: ", path);
				SetBoolResult(pVarResult, ok);
				return S_OK;
			}
		case SKIN_DISPID_OPEN_FILE_DIALOG:
			SetStringResult(pVarResult, SkinEditorFileDialog(m_hwnd, false, GetStringArg(pDispParams, 0)));
			return S_OK;
		case SKIN_DISPID_SAVE_FILE_DIALOG:
			SetStringResult(pVarResult, SkinEditorFileDialog(m_hwnd, true, GetStringArg(pDispParams, 0)));
			return S_OK;
		case SKIN_DISPID_GET_HINT_TEXT:
			// 返回 JSON：{"text": 设置中的提示词, "useTheme": 是否使用主题提供的提示词}，与 refreshSkin 的逻辑一致
			{
				nlohmann::json info;
				info["text"] = g_settings_map["pref_search_box_placeholder"].stringValue;
				info["useTheme"] = g_settings_map["pref_search_box_placeholder_use_theme"].boolValue;
				SetStringResult(pVarResult, utf8_to_wide(info.dump()));
				return S_OK;
			}
		case SKIN_DISPID_GET_FILE_INFO:
			SetStringResult(pVarResult, SkinEditorGetFileInfo(GetStringArg(pDispParams, 0)));
			return S_OK;
		case SKIN_DISPID_REVEAL_IN_EXPLORER:
			SetBoolResult(pVarResult, SkinEditorRevealFile(GetStringArg(pDispParams, 0)));
			return S_OK;
		default:
			return DISP_E_MEMBERNOTFOUND;
		}
	}

	// ---------- IDropTarget ----------
	// 文件拖放由宿主处理（页面拿不到真实路径），其余拖放（如拖动文本）交给浏览器默认处理
	STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
		FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		m_isFileDrag = pDataObj && pDataObj->QueryGetData(&format) == S_OK;
		if (m_isFileDrag) {
			*pdwEffect = DROPEFFECT_COPY;
			CallScript(L"onHostDragState", {L"1"});
			return S_OK;
		}
		return m_defaultDropTarget ? m_defaultDropTarget->DragEnter(pDataObj, grfKeyState, pt, pdwEffect) : S_OK;
	}
	STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
		if (m_isFileDrag) {
			*pdwEffect = DROPEFFECT_COPY;
			return S_OK;
		}
		return m_defaultDropTarget ? m_defaultDropTarget->DragOver(grfKeyState, pt, pdwEffect) : S_OK;
	}
	STDMETHODIMP DragLeave() override {
		if (m_isFileDrag) {
			m_isFileDrag = false;
			CallScript(L"onHostDragState", {L"0"});
			return S_OK;
		}
		return m_defaultDropTarget ? m_defaultDropTarget->DragLeave() : S_OK;
	}
	STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
		if (!m_isFileDrag) {
			return m_defaultDropTarget ? m_defaultDropTarget->Drop(pDataObj, grfKeyState, pt, pdwEffect) : S_OK;
		}
		m_isFileDrag = false;
		CallScript(L"onHostDragState", {L"0"});
		*pdwEffect = DROPEFFECT_NONE;

		FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM medium{};
		if (SUCCEEDED(pDataObj->GetData(&format, &medium))) {
			wchar_t path[MAX_PATH * 4] = {};
			if (DragQueryFileW(static_cast<HDROP>(medium.hGlobal), 0, path, static_cast<UINT>(std::size(path))) > 0) {
				*pdwEffect = DROPEFFECT_COPY;
				// 弹出确认框时不能占着拖放源，延迟到消息循环中再交给页面
				m_pendingDropPath = path;
				PostMessageW(m_hwnd, WM_APP + 1, 0, 0);
			}
			ReleaseStgMedium(&medium);
		}
		return S_OK;
	}

	void DeliverPendingDrop() {
		if (m_pendingDropPath.empty()) return;
		const std::wstring path = std::move(m_pendingDropPath);
		m_pendingDropPath.clear();
		CallScript(L"onHostFileDrop", {path});
	}

private:
	static std::wstring GetStringArg(const DISPPARAMS* params, UINT index) {
		if (!params || index >= params->cArgs) return L"";
		VARIANT converted;
		VariantInit(&converted);
		// 参数倒序存放
		VARIANT* source = &params->rgvarg[params->cArgs - 1 - index];
		std::wstring result;
		if (SUCCEEDED(VariantChangeType(&converted, source, 0, VT_BSTR)) && converted.bstrVal) {
			result.assign(converted.bstrVal, SysStringLen(converted.bstrVal));
		}
		VariantClear(&converted);
		return result;
	}

	static void SetStringResult(VARIANT* result, const std::wstring& value) {
		if (!result) return;
		VariantInit(result);
		result->vt = VT_BSTR;
		result->bstrVal = SysAllocStringLen(value.c_str(), static_cast<UINT>(value.size()));
	}

	static void SetBoolResult(VARIANT* result, bool value) {
		if (!result) return;
		VariantInit(result);
		result->vt = VT_BOOL;
		result->boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
	}

	LONG m_refCount = 1;
	HWND m_hwnd;
	IOleObject* m_oleObject = nullptr;
	IWebBrowser2* m_browser = nullptr;
	IOleInPlaceObject* m_inPlaceObject = nullptr;
	IOleInPlaceActiveObject* m_activeObject = nullptr;
	IDropTarget* m_defaultDropTarget = nullptr;
	bool m_isFileDrag = false;
	std::wstring m_pendingDropPath;
};

inline SkinEditorHost* g_skinEditorHost = nullptr;

// WebBrowser 控件默认使用软件渲染，窗口最大化且高倍缩放时平移画布每帧要重绘 50ms 以上；
// 为本进程开启 IE 的 GPU 渲染（按 exe 文件名生效，须在首次创建控件前写入）
static void SkinEditorEnableGpuRendering() {
	wchar_t exePath[MAX_PATH] = {};
	if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return;
	const wchar_t* exeName = wcsrchr(exePath, L'\\');
	exeName = exeName ? exeName + 1 : exePath;
	HKEY key = nullptr;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, LR"(Software\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_GPU_RENDERING)",
						0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
		Loge(L"SkinEditor", L"无法写入 FEATURE_GPU_RENDERING");
		return;
	}
	const DWORD enabled = 1;
	RegSetValueExW(key, exeName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&enabled), sizeof(enabled));
	RegCloseKey(key);
}

// 取得编辑器页面地址：调试版直接使用源码目录中的页面方便修改，否则把内嵌资源释放到临时目录
static std::wstring GetSkinEditorPageUrl() {
	std::wstring pagePath;
#if defined(DEBUG) || defined(_DEBUG) || !defined(NDEBUG) || defined(REL_WITH_DEB_INFO_DEBUG)
	const std::wstring devPage = EXE_FOLDER_PATH + LR"(\..\web\skin_editor.html)";
	if (GetFileAttributesW(devPage.c_str()) != INVALID_FILE_ATTRIBUTES) {
		pagePath = devPage;
	}
#endif
	if (pagePath.empty()) {
		wchar_t tempDir[MAX_PATH] = {};
		GetTempPathW(MAX_PATH, tempDir);
		pagePath = std::wstring(tempDir) + L"CandyLauncher_skin_editor.html";
		try {
			const std::string html = GetAppResourceText(IDR_SKIN_EDITOR_HTML);
			std::ofstream out(pagePath, std::ios::binary | std::ios::trunc);
			out.write(html.data(), static_cast<std::streamsize>(html.size()));
		} catch (const std::exception& e) {
			Loge(L"SkinEditor", L"读取内嵌编辑器页面失败: ", e.what());
			return L"";
		}
	}
	return L"file:///" + pagePath;
}

// 由主消息循环调用，把键盘消息交给浏览器处理，否则 Tab、Delete、Ctrl+C 等按键在网页中无效
static bool SkinEditorPreTranslateMessage(MSG* msg) {
	if (!g_skinEditorHwnd || !g_skinEditorActiveObject) return false;
	if (msg->message < WM_KEYFIRST || msg->message > WM_KEYLAST) return false;
	if (msg->hwnd != g_skinEditorHwnd && !IsChild(g_skinEditorHwnd, msg->hwnd)) return false;
	return g_skinEditorActiveObject->TranslateAccelerator(msg) == S_OK;
}

static LRESULT CALLBACK SkinEditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		{
			g_skinEditorHost = new SkinEditorHost(hwnd);
			if (!g_skinEditorHost->Create()) {
				MessageBoxW(hwnd, SkinEditorIsZh() ? L"无法创建 WebBrowser 控件" : L"Failed to create the WebBrowser control",
							L"CandyLauncher", MB_OK | MB_ICONERROR);
				return -1;
			}
			const std::wstring url = GetSkinEditorPageUrl();
			if (url.empty()) return -1;
			g_skinEditorHost->Navigate(url);
			return 0;
		}
	case WM_SIZE:
		if (g_skinEditorHost) g_skinEditorHost->Resize();
		return 0;
	case WM_APP + 1:
		if (g_skinEditorHost) g_skinEditorHost->DeliverPendingDrop();
		return 0;
	case WM_CLOSE:
		{
			// 页面有未保存内容时由页面询问是否放弃
			if (g_skinEditorHost) {
				VARIANT result;
				VariantInit(&result);
				if (g_skinEditorHost->CallScript(L"hostQueryClose", {}, &result) &&
					result.vt == VT_BOOL && result.boolVal == VARIANT_FALSE) {
					return 0;
				}
				VariantClear(&result);
			}
			DestroyWindow(hwnd);
			return 0;
		}
	case WM_DESTROY:
		if (g_skinEditorHost) {
			g_skinEditorHost->Destroy();
			g_skinEditorHost->Release();
			g_skinEditorHost = nullptr;
		}
		g_skinEditorHwnd = nullptr;
		return 0;
	default:
		break;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void ShowSkinEditorWindow() {
	if (g_skinEditorHwnd) {
		RestoreWindowIfMinimized(g_skinEditorHwnd);
		SetForegroundWindow(g_skinEditorHwnd);
		return;
	}

	static bool registered = false;
	if (!registered) {
		SkinEditorEnableGpuRendering();
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = SkinEditorWndProc;
		wc.hInstance = g_hInst;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_CANDYLAUNCHER));
		wc.hIconSm = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_CANDYLAUNCHER));
		wc.lpszClassName = L"SkinEditorWndClass";
		registered = RegisterClassExW(&wc) != 0;
	}

	RECT workArea{};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
	const int width = MyMin(SKIN_EDITOR_WIDTH, static_cast<int>(workArea.right - workArea.left));
	const int height = MyMin(SKIN_EDITOR_HEIGHT, static_cast<int>(workArea.bottom - workArea.top));
	const int x = workArea.left + (workArea.right - workArea.left - width) / 2;
	const int y = workArea.top + (workArea.bottom - workArea.top - height) / 2;

	g_skinEditorHwnd = CreateWindowExW(0, L"SkinEditorWndClass",
										SkinEditorIsZh() ? L"皮肤编辑器" : L"Skin Editor",
										WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
										x, y, width, height, nullptr, nullptr, g_hInst, nullptr);
	if (!g_skinEditorHwnd) {
		Loge(L"SkinEditor", L"创建皮肤编辑器窗口失败");
		return;
	}
	ShowWindow(g_skinEditorHwnd, SW_SHOW);
	UpdateWindow(g_skinEditorHwnd);
}
