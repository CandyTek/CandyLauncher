#pragma once

#include <windows.h>
#include <oleidl.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "LogUtil.hpp"

struct OleDragDropData {
	std::vector<std::wstring> filePaths;
	std::wstring text;
	std::wstring url;
	std::wstring html;
	std::wstring rtf;
	HBITMAP bitmap = nullptr;

	bool HasAnyData() const {
		return !filePaths.empty()
			|| !text.empty()
			|| !url.empty()
			|| !html.empty()
			|| !rtf.empty()
			|| bitmap != nullptr;
	}
};

class SimpleDropSource final : public IDropSource {
public:
	ULONG STDMETHODCALLTYPE AddRef() override {
		return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
	}

	ULONG STDMETHODCALLTYPE Release() override {
		const ULONG refCount = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
		if (refCount == 0) {
			delete this;
		}
		return refCount;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
		if (!ppvObject) {
			return E_POINTER;
		}
		*ppvObject = nullptr;
		if (riid == IID_IUnknown || riid == IID_IDropSource) {
			*ppvObject = static_cast<IDropSource*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
		if (fEscapePressed) {
			return DRAGDROP_S_CANCEL;
		}
		if ((grfKeyState & MK_LBUTTON) == 0) {
			return DRAGDROP_S_DROP;
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override {
		return DRAGDROP_S_USEDEFAULTCURSORS;
	}

private:
	LONG m_refCount = 1;
};

class SimpleDataObject final : public IDataObject {
public:
	explicit SimpleDataObject(const OleDragDropData& data) {
		m_cfPreferredDropEffect = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
		m_cfUniformResourceLocatorW = RegisterClipboardFormatW(L"UniformResourceLocatorW");
		m_cfUniformResourceLocator = RegisterClipboardFormatW(L"UniformResourceLocator");
		m_cfHtml = RegisterClipboardFormatW(L"HTML Format");
		m_cfRtf = RegisterClipboardFormatW(L"Rich Text Format");

		if (!data.filePaths.empty()) {
			m_hDrop = CreateDropFilesHGlobal(data.filePaths);
		}
		if (!data.text.empty()) {
			m_unicodeText = CreateWideTextHGlobal(data.text);
			m_ansiText = CreateAnsiTextHGlobal(data.text);
		}
		if (!data.url.empty()) {
			m_unicodeUrl = CreateWideTextHGlobal(data.url);
			m_ansiUrl = CreateAnsiTextHGlobal(data.url);
		}
		if (!data.html.empty()) {
			m_html = CreateUtf8TextHGlobal(BuildClipboardHtml(data.html));
		}
		if (!data.rtf.empty()) {
			m_rtf = CreateAnsiTextHGlobal(data.rtf);
		}
		if (data.bitmap) {
			m_bitmap = static_cast<HBITMAP>(CopyImage(data.bitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
		}

		if (HasPayload()) {
			m_preferredDropEffect = CreatePreferredDropEffectHGlobal((m_hDrop != nullptr) ? DROPEFFECT_COPY : DROPEFFECT_LINK);
		}
	}

	~SimpleDataObject() {
		FreeHGlobal(m_hDrop);
		FreeHGlobal(m_unicodeText);
		FreeHGlobal(m_ansiText);
		FreeHGlobal(m_unicodeUrl);
		FreeHGlobal(m_ansiUrl);
		FreeHGlobal(m_html);
		FreeHGlobal(m_rtf);
		FreeHGlobal(m_preferredDropEffect);
		if (m_bitmap) {
			DeleteObject(m_bitmap);
			m_bitmap = nullptr;
		}
	}

	bool IsValid() const {
		return HasPayload();
	}

	ULONG STDMETHODCALLTYPE AddRef() override {
		return static_cast<ULONG>(InterlockedIncrement(&m_refCount));
	}

	ULONG STDMETHODCALLTYPE Release() override {
		const ULONG refCount = static_cast<ULONG>(InterlockedDecrement(&m_refCount));
		if (refCount == 0) {
			delete this;
		}
		return refCount;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
		if (!ppvObject) {
			return E_POINTER;
		}
		*ppvObject = nullptr;
		if (riid == IID_IUnknown || riid == IID_IDataObject) {
			*ppvObject = static_cast<IDataObject*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) override {
		if (!pformatetcIn || !pmedium) {
			return E_INVALIDARG;
		}
		if (pformatetcIn->cfFormat == CF_BITMAP) {
			if (!(pformatetcIn->tymed & TYMED_GDI) || !m_bitmap) {
				return DV_E_TYMED;
			}
			return DuplicateBitmapToMedium(m_bitmap, pmedium);
		}
		if (!(pformatetcIn->tymed & TYMED_HGLOBAL)) {
			return DV_E_TYMED;
		}
		if (pformatetcIn->cfFormat == CF_HDROP && m_hDrop) {
			return DuplicateHGlobalToMedium(m_hDrop, pmedium);
		}
		if (pformatetcIn->cfFormat == CF_UNICODETEXT && m_unicodeText) {
			return DuplicateHGlobalToMedium(m_unicodeText, pmedium);
		}
		if (pformatetcIn->cfFormat == CF_TEXT && m_ansiText) {
			return DuplicateHGlobalToMedium(m_ansiText, pmedium);
		}
		if (m_cfUniformResourceLocatorW != 0 && pformatetcIn->cfFormat == m_cfUniformResourceLocatorW && m_unicodeUrl) {
			return DuplicateHGlobalToMedium(m_unicodeUrl, pmedium);
		}
		if (m_cfUniformResourceLocator != 0 && pformatetcIn->cfFormat == m_cfUniformResourceLocator && m_ansiUrl) {
			return DuplicateHGlobalToMedium(m_ansiUrl, pmedium);
		}
		if (m_cfHtml != 0 && pformatetcIn->cfFormat == m_cfHtml && m_html) {
			return DuplicateHGlobalToMedium(m_html, pmedium);
		}
		if (m_cfRtf != 0 && pformatetcIn->cfFormat == m_cfRtf && m_rtf) {
			return DuplicateHGlobalToMedium(m_rtf, pmedium);
		}
		if (m_cfPreferredDropEffect != 0 && pformatetcIn->cfFormat == m_cfPreferredDropEffect && m_preferredDropEffect) {
			return DuplicateHGlobalToMedium(m_preferredDropEffect, pmedium);
		}
		return DV_E_FORMATETC;
	}

	HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override {
		return DATA_E_FORMATETC;
	}

	HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pformatetc) override {
		if (!pformatetc) {
			return E_INVALIDARG;
		}
		if (pformatetc->cfFormat == CF_BITMAP) {
			return ((pformatetc->tymed & TYMED_GDI) && m_bitmap) ? S_OK : DV_E_TYMED;
		}
		if (!(pformatetc->tymed & TYMED_HGLOBAL)) {
			return DV_E_TYMED;
		}
		if (pformatetc->cfFormat == CF_HDROP && m_hDrop) {
			return S_OK;
		}
		if (pformatetc->cfFormat == CF_UNICODETEXT && m_unicodeText) {
			return S_OK;
		}
		if (pformatetc->cfFormat == CF_TEXT && m_ansiText) {
			return S_OK;
		}
		if (m_cfUniformResourceLocatorW != 0 && pformatetc->cfFormat == m_cfUniformResourceLocatorW && m_unicodeUrl) {
			return S_OK;
		}
		if (m_cfUniformResourceLocator != 0 && pformatetc->cfFormat == m_cfUniformResourceLocator && m_ansiUrl) {
			return S_OK;
		}
		if (m_cfHtml != 0 && pformatetc->cfFormat == m_cfHtml && m_html) {
			return S_OK;
		}
		if (m_cfRtf != 0 && pformatetc->cfFormat == m_cfRtf && m_rtf) {
			return S_OK;
		}
		if (m_cfPreferredDropEffect != 0 && pformatetc->cfFormat == m_cfPreferredDropEffect && m_preferredDropEffect) {
			return S_OK;
		}
		return DV_E_FORMATETC;
	}

	HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* pformatetcOut) override {
		if (pformatetcOut) {
			pformatetcOut->ptd = nullptr;
		}
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override {
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override {
		if (!ppenumFormatEtc) {
			return E_POINTER;
		}
		*ppenumFormatEtc = nullptr;
		if (dwDirection != DATADIR_GET) {
			return E_NOTIMPL;
		}

		std::vector<FORMATETC> formats;
		AddFormat(formats, CF_HDROP, TYMED_HGLOBAL, m_hDrop != nullptr);
		AddFormat(formats, CF_UNICODETEXT, TYMED_HGLOBAL, m_unicodeText != nullptr);
		AddFormat(formats, CF_TEXT, TYMED_HGLOBAL, m_ansiText != nullptr);
		AddFormat(formats, static_cast<CLIPFORMAT>(m_cfUniformResourceLocatorW), TYMED_HGLOBAL,
			m_cfUniformResourceLocatorW != 0 && m_unicodeUrl != nullptr);
		AddFormat(formats, static_cast<CLIPFORMAT>(m_cfUniformResourceLocator), TYMED_HGLOBAL,
			m_cfUniformResourceLocator != 0 && m_ansiUrl != nullptr);
		AddFormat(formats, static_cast<CLIPFORMAT>(m_cfHtml), TYMED_HGLOBAL, m_cfHtml != 0 && m_html != nullptr);
		AddFormat(formats, static_cast<CLIPFORMAT>(m_cfRtf), TYMED_HGLOBAL, m_cfRtf != 0 && m_rtf != nullptr);
		AddFormat(formats, static_cast<CLIPFORMAT>(m_cfPreferredDropEffect), TYMED_HGLOBAL,
			m_cfPreferredDropEffect != 0 && m_preferredDropEffect != nullptr);
		AddFormat(formats, CF_BITMAP, TYMED_GDI, m_bitmap != nullptr);

		return SHCreateStdEnumFmtEtc(static_cast<UINT>(formats.size()), formats.data(), ppenumFormatEtc);
	}

	HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override {
		return OLE_E_ADVISENOTSUPPORTED;
	}

	HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override {
		return OLE_E_ADVISENOTSUPPORTED;
	}

	HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override {
		return OLE_E_ADVISENOTSUPPORTED;
	}

private:
	static void FreeHGlobal(HGLOBAL& handle) {
		if (handle) {
			GlobalFree(handle);
			handle = nullptr;
		}
	}

	bool HasPayload() const {
		return m_hDrop || m_unicodeText || m_ansiText || m_unicodeUrl || m_ansiUrl || m_html || m_rtf || m_bitmap;
	}

	static void AddFormat(std::vector<FORMATETC>& formats, CLIPFORMAT clipFormat, DWORD tymed, bool enabled) {
		if (!enabled || clipFormat == 0) {
			return;
		}
		FORMATETC format{};
		format.cfFormat = clipFormat;
		format.dwAspect = DVASPECT_CONTENT;
		format.lindex = -1;
		format.tymed = tymed;
		formats.push_back(format);
	}

	static HGLOBAL CreateDropFilesHGlobal(const std::vector<std::wstring>& filePaths) {
		std::vector<std::wstring> validPaths;
		validPaths.reserve(filePaths.size());
		for (const auto& path : filePaths) {
			if (!path.empty()) {
				validPaths.push_back(path);
			}
		}
		if (validPaths.empty()) {
			return nullptr;
		}

		size_t charCount = 1;
		for (const auto& path : validPaths) {
			charCount += path.size() + 1;
		}

		const SIZE_T bytes = sizeof(DROPFILES) + charCount * sizeof(wchar_t);
		HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, bytes);
		if (!hGlobal) {
			return nullptr;
		}

		auto* dropFiles = static_cast<DROPFILES*>(GlobalLock(hGlobal));
		if (!dropFiles) {
			GlobalFree(hGlobal);
			return nullptr;
		}

		dropFiles->pFiles = sizeof(DROPFILES);
		dropFiles->pt = POINT{0, 0};
		dropFiles->fNC = FALSE;
		dropFiles->fWide = TRUE;

		auto* buffer = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(dropFiles) + sizeof(DROPFILES));
		for (const auto& path : validPaths) {
			memcpy(buffer, path.c_str(), path.size() * sizeof(wchar_t));
			buffer += path.size();
			*buffer++ = L'\0';
		}
		*buffer = L'\0';
		GlobalUnlock(hGlobal);
		return hGlobal;
	}

	static HGLOBAL CreateWideTextHGlobal(const std::wstring& text) {
		const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
		HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, bytes);
		if (!hGlobal) {
			return nullptr;
		}
		auto* buffer = static_cast<wchar_t*>(GlobalLock(hGlobal));
		if (!buffer) {
			GlobalFree(hGlobal);
			return nullptr;
		}
		memcpy(buffer, text.c_str(), text.size() * sizeof(wchar_t));
		buffer[text.size()] = L'\0';
		GlobalUnlock(hGlobal);
		return hGlobal;
	}

	static std::string WideToAnsi(const std::wstring& text, UINT codePage = CP_ACP) {
		if (text.empty()) {
			return {};
		}
		const int bufferSize = WideCharToMultiByte(codePage, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (bufferSize <= 0) {
			return {};
		}
		std::string result(static_cast<size_t>(bufferSize), '\0');
		WideCharToMultiByte(codePage, 0, text.c_str(), -1, result.data(), bufferSize, nullptr, nullptr);
		if (!result.empty() && result.back() == '\0') {
			result.pop_back();
		}
		return result;
	}

	static HGLOBAL CreateAnsiTextHGlobal(const std::wstring& text) {
		return CreateRawBytesHGlobal(WideToAnsi(text, CP_ACP), true);
	}

	static HGLOBAL CreateUtf8TextHGlobal(const std::string& text) {
		return CreateRawBytesHGlobal(text, true);
	}

	static HGLOBAL CreateRawBytesHGlobal(const std::string& text, bool appendNullTerminator) {
		const SIZE_T bytes = text.size() + (appendNullTerminator ? 1 : 0);
		HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, bytes == 0 ? 1 : bytes);
		if (!hGlobal) {
			return nullptr;
		}
		auto* buffer = static_cast<char*>(GlobalLock(hGlobal));
		if (!buffer) {
			GlobalFree(hGlobal);
			return nullptr;
		}
		if (!text.empty()) {
			memcpy(buffer, text.data(), text.size());
		}
		if (appendNullTerminator) {
			buffer[text.size()] = '\0';
		}
		GlobalUnlock(hGlobal);
		return hGlobal;
	}

	static std::string BuildClipboardHtml(const std::wstring& htmlFragment) {
		const std::string utf8Fragment = WideToAnsi(htmlFragment, CP_UTF8);
		const std::string startFragmentTag = "<!--StartFragment-->";
		const std::string endFragmentTag = "<!--EndFragment-->";
		const std::string body = "<html><body>" + startFragmentTag + utf8Fragment + endFragmentTag + "</body></html>";

		auto formatOffset = [](size_t value) {
			char buffer[16];
			sprintf_s(buffer, "%010zu", value);
			return std::string(buffer);
		};

		std::string header =
			"Version:0.9\r\n"
			"StartHTML:0000000000\r\n"
			"EndHTML:0000000000\r\n"
			"StartFragment:0000000000\r\n"
			"EndFragment:0000000000\r\n";

		const size_t startHtml = header.size();
		const size_t startFragment = startHtml + body.find(startFragmentTag) + startFragmentTag.size();
		const size_t endFragment = startHtml + body.find(endFragmentTag);
		const size_t endHtml = startHtml + body.size();

		header.replace(header.find("StartHTML:") + 10, 10, formatOffset(startHtml));
		header.replace(header.find("EndHTML:") + 8, 10, formatOffset(endHtml));
		header.replace(header.find("StartFragment:") + 14, 10, formatOffset(startFragment));
		header.replace(header.find("EndFragment:") + 12, 10, formatOffset(endFragment));

		return header + body;
	}

	static HGLOBAL CreatePreferredDropEffectHGlobal(const DWORD dropEffect) {
		HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, sizeof(DWORD));
		if (!hGlobal) {
			return nullptr;
		}

		auto* data = static_cast<DWORD*>(GlobalLock(hGlobal));
		if (!data) {
			GlobalFree(hGlobal);
			return nullptr;
		}
		*data = dropEffect;
		GlobalUnlock(hGlobal);
		return hGlobal;
	}

	static HRESULT DuplicateHGlobalToMedium(HGLOBAL source, STGMEDIUM* medium) {
		if (!source || !medium) {
			return E_INVALIDARG;
		}

		const SIZE_T size = GlobalSize(source);
		HGLOBAL copy = GlobalAlloc(GHND | GMEM_SHARE, size);
		if (!copy) {
			return E_OUTOFMEMORY;
		}

		void* srcPtr = GlobalLock(source);
		void* dstPtr = GlobalLock(copy);
		if (!srcPtr || !dstPtr) {
			if (srcPtr) {
				GlobalUnlock(source);
			}
			if (dstPtr) {
				GlobalUnlock(copy);
			}
			GlobalFree(copy);
			return E_OUTOFMEMORY;
		}

		memcpy(dstPtr, srcPtr, size);
		GlobalUnlock(copy);
		GlobalUnlock(source);

		medium->tymed = TYMED_HGLOBAL;
		medium->hGlobal = copy;
		medium->pUnkForRelease = nullptr;
		return S_OK;
	}

	static HRESULT DuplicateBitmapToMedium(HBITMAP source, STGMEDIUM* medium) {
		if (!source || !medium) {
			return E_INVALIDARG;
		}

		HBITMAP copy = static_cast<HBITMAP>(CopyImage(source, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
		if (!copy) {
			return E_OUTOFMEMORY;
		}

		medium->tymed = TYMED_GDI;
		medium->hBitmap = copy;
		medium->pUnkForRelease = nullptr;
		return S_OK;
	}

	LONG m_refCount = 1;
	HGLOBAL m_hDrop = nullptr;
	HGLOBAL m_unicodeText = nullptr;
	HGLOBAL m_ansiText = nullptr;
	HGLOBAL m_unicodeUrl = nullptr;
	HGLOBAL m_ansiUrl = nullptr;
	HGLOBAL m_html = nullptr;
	HGLOBAL m_rtf = nullptr;
	HGLOBAL m_preferredDropEffect = nullptr;
	HBITMAP m_bitmap = nullptr;
	UINT m_cfPreferredDropEffect = 0;
	UINT m_cfUniformResourceLocatorW = 0;
	UINT m_cfUniformResourceLocator = 0;
	UINT m_cfHtml = 0;
	UINT m_cfRtf = 0;
};

inline std::atomic<bool> g_isOleFileDragDropInProgress = false;

inline bool BeginOleDataDragDrop(const OleDragDropData& data, DWORD* performedEffect = nullptr) {
	if (!data.HasAnyData()) {
		Logi(L"OleDragDrop", L"Skip empty drag data");
		return false;
	}

	auto* dataObject = new SimpleDataObject(data);
	if (!dataObject->IsValid()) {
		dataObject->Release();
		Logi(L"OleDragDrop", L"Create data object failed");
		return false;
	}

	auto* dropSource = new SimpleDropSource();
	DWORD effect = DROPEFFECT_NONE;
	g_isOleFileDragDropInProgress = true;
	const HRESULT hr = DoDragDrop(dataObject, dropSource, DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
	g_isOleFileDragDropInProgress = false;

	dropSource->Release();
	dataObject->Release();

	if (performedEffect) {
		*performedEffect = effect;
	}
	Logi(
		L"OleDragDrop",
		L"DoDragDrop hr=" + std::to_wstring(static_cast<long long>(hr)) +
		L", effect=" + std::to_wstring(effect)
	);
	return hr == DRAGDROP_S_DROP && effect != DROPEFFECT_NONE;
}

inline bool BeginOleFileDragDrop(const std::vector<std::wstring>& filePaths, DWORD* performedEffect = nullptr) {
	OleDragDropData data;
	data.filePaths = filePaths;
	return BeginOleDataDragDrop(data, performedEffect);
}
