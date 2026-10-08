#pragma once

#include <windows.h>
#include <oleidl.h>
#include <shellapi.h>

#include <functional>
#include <string>
#include <vector>

// 让普通 EDIT 控件接受 OLE 拖放的文本和文件。
// EDIT 本身只支持 WM_DROPFILES（仅文件），从浏览器、编辑器或本程序列表拖出的文本无法放入。
class EditDropTarget final : public IDropTarget {
public:
	// clientPt 为相对 EDIT 客户区的放下位置
	using TextHandler = std::function<void(const std::wstring& text, POINT clientPt)>;
	using FilesHandler = std::function<void(const std::vector<std::wstring>& files)>;

	EditDropTarget(HWND editHwnd, TextHandler onText, FilesHandler onFiles)
		: m_hwnd(editHwnd), m_onText(std::move(onText)), m_onFiles(std::move(onFiles)) {
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
		if (!ppv) return E_POINTER;
		if (riid == IID_IUnknown || riid == IID_IDropTarget) {
			*ppv = static_cast<IDropTarget*>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() override {
		return InterlockedIncrement(&m_refCount);
	}

	ULONG STDMETHODCALLTYPE Release() override {
		const ULONG count = InterlockedDecrement(&m_refCount);
		if (count == 0) delete this;
		return count;
	}

	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* dataObject, DWORD, POINTL, DWORD* effect) override {
		m_acceptable = HasFormat(dataObject, CF_UNICODETEXT) || HasFormat(dataObject, CF_TEXT) || HasFormat(dataObject, CF_HDROP);
		*effect = ChooseEffect(*effect);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* effect) override {
		*effect = ChooseEffect(*effect);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DragLeave() override {
		m_acceptable = false;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Drop(IDataObject* dataObject, DWORD, POINTL pt, DWORD* effect) override {
		*effect = ChooseEffect(*effect);
		m_acceptable = false;
		if (*effect == DROPEFFECT_NONE) return S_OK;

		if (std::vector<std::wstring> files = ReadFiles(dataObject); !files.empty()) {
			if (m_onFiles) m_onFiles(files);
			return S_OK;
		}
		std::wstring text;
		if (ReadText(dataObject, text) && !text.empty() && m_onText) {
			POINT clientPt{pt.x, pt.y};
			ScreenToClient(m_hwnd, &clientPt);
			m_onText(text, clientPt);
		} else {
			*effect = DROPEFFECT_NONE;
		}
		return S_OK;
	}

private:
	~EditDropTarget() = default;

	DWORD ChooseEffect(const DWORD allowed) const {
		if (!m_acceptable) return DROPEFFECT_NONE;
		if (allowed & DROPEFFECT_COPY) return DROPEFFECT_COPY;
		if (allowed & DROPEFFECT_LINK) return DROPEFFECT_LINK;
		return DROPEFFECT_NONE;
	}

	static bool HasFormat(IDataObject* dataObject, const CLIPFORMAT format) {
		FORMATETC fmt{format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		return dataObject && dataObject->QueryGetData(&fmt) == S_OK;
	}

	static bool ReadText(IDataObject* dataObject, std::wstring& out) {
		FORMATETC fmt{CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM medium{};
		if (SUCCEEDED(dataObject->GetData(&fmt, &medium))) {
			if (const auto* text = static_cast<const wchar_t*>(GlobalLock(medium.hGlobal))) {
				out.assign(text, wcsnlen(text, GlobalSize(medium.hGlobal) / sizeof(wchar_t)));
				GlobalUnlock(medium.hGlobal);
			}
			ReleaseStgMedium(&medium);
			return true;
		}
		fmt.cfFormat = CF_TEXT;
		if (SUCCEEDED(dataObject->GetData(&fmt, &medium))) {
			if (const auto* text = static_cast<const char*>(GlobalLock(medium.hGlobal))) {
				const int len = static_cast<int>(strnlen(text, GlobalSize(medium.hGlobal)));
				const int wlen = MultiByteToWideChar(CP_ACP, 0, text, len, nullptr, 0);
				out.resize(wlen);
				MultiByteToWideChar(CP_ACP, 0, text, len, out.data(), wlen);
				GlobalUnlock(medium.hGlobal);
			}
			ReleaseStgMedium(&medium);
			return true;
		}
		return false;
	}

	static std::vector<std::wstring> ReadFiles(IDataObject* dataObject) {
		std::vector<std::wstring> files;
		FORMATETC fmt{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM medium{};
		if (FAILED(dataObject->GetData(&fmt, &medium))) return files;
		if (const auto hDrop = static_cast<HDROP>(GlobalLock(medium.hGlobal))) {
			const UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
			for (UINT i = 0; i < count; ++i) {
				const UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
				std::wstring path(len, L'\0');
				DragQueryFileW(hDrop, i, path.data(), len + 1);
				files.push_back(path);
			}
			GlobalUnlock(medium.hGlobal);
		}
		ReleaseStgMedium(&medium);
		return files;
	}

	LONG m_refCount = 1;
	HWND m_hwnd;
	TextHandler m_onText;
	FilesHandler m_onFiles;
	bool m_acceptable = false;
};

// 单行 EDIT 不能包含换行，拖入的多行文本合并为一行
inline std::wstring NormalizeDroppedEditText(std::wstring text) {
	for (auto& c : text) {
		if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
	}
	while (!text.empty() && text.back() == L' ') text.pop_back();
	return text;
}

// 在放下位置插入文本，EM_REPLACESEL 会触发 EN_CHANGE，从而刷新搜索结果
inline void InsertTextIntoEditAtPoint(HWND editHwnd, const std::wstring& text, const POINT clientPt) {
	const LRESULT hit = SendMessageW(editHwnd, EM_CHARFROMPOS, 0, MAKELPARAM(clientPt.x, clientPt.y));
	DWORD pos = LOWORD(hit);
	if (hit == -1) pos = static_cast<DWORD>(GetWindowTextLengthW(editHwnd));
	SendMessageW(editHwnd, EM_SETSEL, pos, pos);
	SendMessageW(editHwnd, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(NormalizeDroppedEditText(text).c_str()));
	SetFocus(editHwnd);
}

inline bool RegisterEditDropTarget(HWND editHwnd, EditDropTarget::TextHandler onText, EditDropTarget::FilesHandler onFiles) {
	auto* target = new EditDropTarget(editHwnd, std::move(onText), std::move(onFiles));
	const HRESULT hr = RegisterDragDrop(editHwnd, target);
	// RegisterDragDrop 成功时自行 AddRef，这里释放创建时的引用
	target->Release();
	return SUCCEEDED(hr);
}
