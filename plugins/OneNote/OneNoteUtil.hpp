#pragma once

#include "OneNoteAction.hpp"
#include <comdef.h>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <msxml6.h>

#include "util/StringUtil.hpp"
#include "util/LogUtil.hpp"
#include "util/BitmapUtil.hpp"

#pragma comment(lib, "msxml6.lib")

// OneNote Application CLSID (from registry)
static const CLSID CLSID_OneNoteApplication = {
	0xDC67E480, 0xC3CB, 0x49F8,
	{0x82, 0x32, 0x60, 0xB0, 0xC2, 0x05, 0x6C, 0x8E}
};

// HierarchyScope enum (from OneNote API)
enum HierarchyScope {
	hsSelf = 0,
	hsChildren = 1,
	hsNotebooks = 2,
	hsSections = 3,
	hsPages = 4
};

// XMLSchema enum (from OneNote API)
enum XMLSchema {
	xs2007 = 0,
	xs2010 = 1,
	xs2013 = 2,
	xsCurrent = 3
};

// OneNote COM 辅助类
class OneNoteHelper {
private:
	IDispatch* m_oneNoteApp = nullptr;
	bool m_initialized = false;

public:
	OneNoteHelper() {
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
			Loge(L"OneNote", L"Failed to initialize COM");
			return;
		}

		// Try different ProgIDs for different OneNote versions
		CLSID clsid;
		hr = CLSIDFromProgID(L"OneNote.Application.15", &clsid);  // Try Office 2013/2016 first
		if (FAILED(hr)) {
			ConsolePrintln(L"OneNote", L"OneNote.Application.15 not found, trying OneNote.Application");
			hr = CLSIDFromProgID(L"OneNote.Application", &clsid);
		}
		if (FAILED(hr)) {
			// ConsolePrintln(L"OneNote", L"Failed to get CLSID from ProgID (HRESULT: 0x" +
			// 	std::to_wstring(hr) + L"), using hardcoded CLSID");
			wchar_t buf[32]{};
			swprintf(buf, 32, L"0x%08lX", static_cast<unsigned long>(hr));
			ConsolePrintln(L"OneNote", std::wstring(L"Failed to get CLSID from ProgID (HRESULT: ") + buf + L")");
			clsid = CLSID_OneNoteApplication;
		}

		// Create OneNote COM instance using IDispatch interface
		hr = CoCreateInstance(
			clsid,
			nullptr,
			CLSCTX_LOCAL_SERVER,
			IID_IDispatch,
			reinterpret_cast<void**>(&m_oneNoteApp)
		);

		if (FAILED(hr)) {
			ConsolePrintln(L"OneNote", L"OneNote is not installed or not available (HRESULT: 0x" +
				std::to_wstring(hr) + L")");
			m_initialized = false;
		} else {
			m_initialized = true;
			ConsolePrintln(L"OneNote", L"Successfully connected to OneNote COM interface");

			// Debug: Try to list available methods
			ITypeInfo* pTypeInfo = nullptr;
			unsigned int typeInfoCount = 0;
			if (SUCCEEDED(m_oneNoteApp->GetTypeInfoCount(&typeInfoCount))) {
				ConsolePrintln(L"OneNote", L"TypeInfo count: " + std::to_wstring(typeInfoCount));

				if (typeInfoCount > 0) {
					HRESULT hrTypeInfo = m_oneNoteApp->GetTypeInfo(0, LOCALE_USER_DEFAULT, &pTypeInfo);
					if (SUCCEEDED(hrTypeInfo) && pTypeInfo) {
						TYPEATTR* pTypeAttr = nullptr;
						if (SUCCEEDED(pTypeInfo->GetTypeAttr(&pTypeAttr))) {
							ConsolePrintln(L"OneNote", L"TypeInfo available with " +
								std::to_wstring(pTypeAttr->cFuncs) + L" functions, cVars=" +
								std::to_wstring(pTypeAttr->cVars));

							// List first 10 function names for debugging
							for (UINT i = 0; i < std::min(10u, static_cast<UINT>(pTypeAttr->cFuncs)); i++) {
								FUNCDESC* pFuncDesc = nullptr;
								if (SUCCEEDED(pTypeInfo->GetFuncDesc(i, &pFuncDesc))) {
									BSTR funcName = nullptr;
									unsigned int nameCount = 0;
									if (SUCCEEDED(pTypeInfo->GetNames(pFuncDesc->memid, &funcName, 1, &nameCount))) {
										if (funcName) {
											ConsolePrintln(L"OneNote", L"  [" + std::to_wstring(i) + L"] DISPID=" +
												std::to_wstring(pFuncDesc->memid) + L": " + std::wstring(funcName));
											SysFreeString(funcName);
										}
									}
									pTypeInfo->ReleaseFuncDesc(pFuncDesc);
								}
							}

							pTypeInfo->ReleaseTypeAttr(pTypeAttr);
						} else {
							ConsolePrintln(L"OneNote", L"Failed to get TypeAttr");
						}
						pTypeInfo->Release();
					} else {
						ConsolePrintln(L"OneNote", L"Failed to get TypeInfo (HRESULT: 0x" + std::to_wstring(hrTypeInfo) + L")");
					}
				}
			} else {
				ConsolePrintln(L"OneNote", L"Failed to get TypeInfo count");
			}
		}
	}

	~OneNoteHelper() {
		if (m_oneNoteApp) {
			m_oneNoteApp->Release();
			m_oneNoteApp = nullptr;
		}
		// CoUninitialize(); // 不调用，避免影响其他 COM 组件
	}

	bool IsInitialized() const {
		return m_initialized;
	}

private:
	// Helper function to invoke IDispatch methods by name
	// Uses ITypeInfo::Invoke instead of IDispatch::Invoke for better compatibility
	HRESULT InvokeMethodByName(const wchar_t* methodName, VARIANT* pVarResult, DISPPARAMS* pDispParams) {
		if (!m_oneNoteApp) {
			return E_POINTER;
		}

		// Load type library directly
		static const GUID LIBID_OneNote = {0x0EA692EE, 0xBB50, 0x4E3C, {0xAE, 0xF0, 0x35, 0x6D, 0x91, 0x73, 0x27, 0x25}};

		ITypeLib* pTypeLib = nullptr;
		HRESULT hrLib = LoadRegTypeLib(
			LIBID_OneNote,
			1, 1,  // Version 1.1
			0,     // LCID = 0 (neutral locale)
			&pTypeLib
		);

		if (FAILED(hrLib) || !pTypeLib) {
			ConsolePrintln(L"OneNote", L"Failed to load TypeLib (HRESULT: 0x" + std::to_wstring(hrLib) + L")");
			return hrLib;
		}

		// Find the interface with the method
		UINT typeInfoCount = pTypeLib->GetTypeInfoCount();
		ITypeInfo* pCorrectTypeInfo = nullptr;
		DISPID dispid = 0;

		for (UINT idx = 0; idx < typeInfoCount && !pCorrectTypeInfo; idx++) {
			ITypeInfo* pTI = nullptr;
			if (SUCCEEDED(pTypeLib->GetTypeInfo(idx, &pTI))) {
				TYPEATTR* pTA = nullptr;
				if (SUCCEEDED(pTI->GetTypeAttr(&pTA))) {
					// Check if this is an interface or dispatch interface
					if (pTA->typekind == TKIND_INTERFACE || pTA->typekind == TKIND_DISPATCH) {
						// Try to find the method
						LPOLESTR name = const_cast<LPOLESTR>(methodName);
						HRESULT hrGetId = pTI->GetIDsOfNames(&name, 1, &dispid);
						if (SUCCEEDED(hrGetId)) {
							pCorrectTypeInfo = pTI;
							pCorrectTypeInfo->AddRef();
							ConsolePrintln(L"OneNote", L"Found method '" + std::wstring(methodName) +
								L"' in TypeInfo[" + std::to_wstring(idx) + L"]");
						}
					}
					pTI->ReleaseTypeAttr(pTA);
				}
				if (!pCorrectTypeInfo) {
					pTI->Release();
				}
			}
		}

		pTypeLib->Release();

		if (!pCorrectTypeInfo) {
			ConsolePrintln(L"OneNote", L"Could not find interface with method: " + std::wstring(methodName));
			return E_FAIL;
		}

		// Use ITypeInfo::Invoke instead of IDispatch::Invoke
		EXCEPINFO excepInfo;
		UINT argErr = 0;
		ZeroMemory(&excepInfo, sizeof(excepInfo));

		// Get FUNCDESC to determine invkind
		FUNCDESC* pFuncDesc = nullptr;
		TYPEATTR* pTypeAttr = nullptr;
		WORD invkind = INVOKE_FUNC;  // Default to INVOKE_FUNC

		if (SUCCEEDED(pCorrectTypeInfo->GetTypeAttr(&pTypeAttr))) {
			for (UINT i = 0; i < pTypeAttr->cFuncs; i++) {
				if (SUCCEEDED(pCorrectTypeInfo->GetFuncDesc(i, &pFuncDesc))) {
					if (pFuncDesc->memid == dispid) {
						invkind = pFuncDesc->invkind;
						ConsolePrintln(L"OneNote", L"Method '" + std::wstring(methodName) +
							L"' has " + std::to_wstring(pFuncDesc->cParams) + L" parameters, invkind=" +
							std::to_wstring(invkind));
						pCorrectTypeInfo->ReleaseFuncDesc(pFuncDesc);
						break;
					}
					pCorrectTypeInfo->ReleaseFuncDesc(pFuncDesc);
				}
			}
			pCorrectTypeInfo->ReleaseTypeAttr(pTypeAttr);
		}

		// Try IDispatch::Invoke directly with LCID=0 instead of ITypeInfo::Invoke
		// This may work better with some OneNote installations
		HRESULT hrInvoke = m_oneNoteApp->Invoke(
			dispid,           // Member to invoke
			IID_NULL,         // Reserved, must be IID_NULL
			0,                // LCID = 0 (neutral locale)
			DISPATCH_METHOD,  // Invoke as method
			pDispParams,      // Parameters
			pVarResult,       // Result
			&excepInfo,       // Exception info
			&argErr           // Argument error index
		);

		if (FAILED(hrInvoke)) {
			wchar_t hexStr[32];
			swprintf_s(hexStr, L"0x%08X", static_cast<unsigned int>(hrInvoke));
			ConsolePrintln(L"OneNote", std::wstring(L"IDispatch::Invoke failed for '") +
				methodName + L"' with HRESULT: " + hexStr);

			if (excepInfo.bstrDescription) {
				ConsolePrintln(L"OneNote", std::wstring(L"  Description: ") +
					std::wstring(excepInfo.bstrDescription));
				SysFreeString(excepInfo.bstrDescription);
			}
			if (excepInfo.bstrSource) {
				ConsolePrintln(L"OneNote", std::wstring(L"  Source: ") +
					std::wstring(excepInfo.bstrSource));
				SysFreeString(excepInfo.bstrSource);
			}
			if (hrInvoke == DISP_E_PARAMNOTFOUND || hrInvoke == DISP_E_TYPEMISMATCH) {
				ConsolePrintln(L"OneNote", L"  Parameter error at index: " + std::to_wstring(argErr));
			}
		}

		pCorrectTypeInfo->Release();
		return hrInvoke;
	}

public:
	// 获取 OneNote 层次结构 XML
	std::wstring GetHierarchy(const std::wstring& startNodeId, enum HierarchyScope scope) {
		if (!m_oneNoteApp) {
			return L"";
		}

		// Prepare parameters in REVERSE order for DISPPARAMS
		// Official signature: GetHierarchy(BSTR bstrStartNodeID, HierarchyScope hsScope,
		//                                  BSTR* pbstrHierarchyXmlOut, XMLSchema xsSchema)
		// rgvarg order: xsSchema -> outXml -> scope -> startNodeId
		VARIANT args[4];
		BSTR bstrXml = nullptr;

		// args[0] = xsSchema (fourth parameter, optional with default value)
		// Use VT_ERROR with DISP_E_PARAMNOTFOUND to indicate "use default value"
		VariantInit(&args[0]);
		args[0].vt = VT_ERROR;
		args[0].scode = DISP_E_PARAMNOTFOUND;

		// args[1] = pbstrHierarchyXmlOut (third parameter, output BSTR*)
		VariantInit(&args[1]);
		args[1].vt = VT_BYREF | VT_BSTR;
		args[1].pbstrVal = &bstrXml;

		// args[2] = hsScope (second parameter)
		VariantInit(&args[2]);
		args[2].vt = VT_I4;
		args[2].lVal = static_cast<long>(scope);

		// args[3] = bstrStartNodeID (first parameter - empty string for "all notebooks")
		VariantInit(&args[3]);
		args[3].vt = VT_BSTR;
		args[3].bstrVal = SysAllocString(startNodeId.empty() ? L"" : startNodeId.c_str());

		DISPPARAMS params;
		params.rgvarg = args;
		params.rgdispidNamedArgs = nullptr;
		params.cArgs = 4;
		params.cNamedArgs = 0;

		VARIANT varResult;
		VariantInit(&varResult);

		// Call GetHierarchy
		HRESULT hr = InvokeMethodByName(L"GetHierarchy", &varResult, &params);

		// Clean up
		if (args[3].bstrVal) {
			SysFreeString(args[3].bstrVal);
		}
		VariantClear(&varResult);

		if (FAILED(hr)) {
			wchar_t buf[32]{};
			swprintf(buf, 32, L"0x%08lX", static_cast<unsigned long>(hr));
			Loge(L"OneNote", std::wstring(L"Failed to get hierarchy (HRESULT: 0x") + buf + L")");
			if (bstrXml) {
				SysFreeString(bstrXml);
			}
			return L"";
		}

		if (!bstrXml) {
			ConsolePrintln(L"OneNote", L"GetHierarchy succeeded but returned null XML");
			return L"";
		}

		std::wstring result(bstrXml);
		SysFreeString(bstrXml);

		return result;
	}

	// 导航到指定页面
	bool NavigateToPage(const std::wstring& pageId) {
		if (!m_oneNoteApp) {
			return false;
		}

		// Prepare parameters - NavigateTo only takes one parameter (pageId)
		VARIANT args[1];
		VariantInit(&args[0]);
		args[0].vt = VT_BSTR;
		args[0].bstrVal = SysAllocString(pageId.c_str());

		DISPPARAMS params;
		params.rgvarg = args;
		params.rgdispidNamedArgs = nullptr;
		params.cArgs = 1;
		params.cNamedArgs = 0;

		VARIANT varResult;
		VariantInit(&varResult);

		// Call NavigateTo
		HRESULT hr = InvokeMethodByName(L"NavigateTo", &varResult, &params);

		SysFreeString(args[0].bstrVal);
		VariantClear(&varResult);

		if (FAILED(hr)) {
			Loge(L"OneNote", L"Failed to navigate to page (HRESULT: 0x" + std::to_wstring(hr) + L")");
			return false;
		}

		return true;
	}
};

// 从页面XML中提取文本内容
static std::wstring ExtractTextFromPageXML(const std::wstring& pageXml) {
	std::wstring result;

	try {
		// 初始化 COM XML
		IXMLDOMDocument2* pXMLDoc = nullptr;
		HRESULT hr = CoCreateInstance(
			__uuidof(DOMDocument60),
			nullptr,
			CLSCTX_INPROC_SERVER,
			__uuidof(IXMLDOMDocument2),
			reinterpret_cast<void**>(&pXMLDoc)
		);

		if (FAILED(hr) || !pXMLDoc) {
			return result;
		}

		// 加载 XML
		VARIANT_BOOL isSuccessful;
		BSTR bstrXml = SysAllocString(pageXml.c_str());
		hr = pXMLDoc->loadXML(bstrXml, &isSuccessful);
		SysFreeString(bstrXml);

		if (FAILED(hr) || isSuccessful == VARIANT_FALSE) {
			pXMLDoc->Release();
			return result;
		}

		// 设置命名空间
		BSTR bstrNamespace = SysAllocString(L"xmlns:one='http://schemas.microsoft.com/office/onenote/2013/onenote'");
		hr = pXMLDoc->setProperty(_bstr_t(L"SelectionNamespaces"), _variant_t(bstrNamespace));
		SysFreeString(bstrNamespace);

		// 查询所有文本节点 (one:T)
		IXMLDOMNodeList* pTextNodes = nullptr;
		BSTR bstrQuery = SysAllocString(L"//one:T");
		hr = pXMLDoc->selectNodes(bstrQuery, &pTextNodes);
		SysFreeString(bstrQuery);

		if (SUCCEEDED(hr) && pTextNodes) {
			long nodeCount = 0;
			pTextNodes->get_length(&nodeCount);

			for (long i = 0; i < nodeCount; ++i) {
				IXMLDOMNode* pTextNode = nullptr;
				hr = pTextNodes->get_item(i, &pTextNode);

				if (SUCCEEDED(hr) && pTextNode) {
					BSTR bstrText = nullptr;
					hr = pTextNode->get_text(&bstrText);

					if (SUCCEEDED(hr) && bstrText) {
						if (!result.empty()) {
							result += L" ";
						}
						result += bstrText;
						SysFreeString(bstrText);
					}

					pTextNode->Release();
				}
			}

			pTextNodes->Release();
		}

		pXMLDoc->Release();

	} catch (const std::exception& e) {
		Loge(L"OneNote", L"Exception in ExtractTextFromPageXML: ", e.what());
	}

	return result;
}

// 解析 XML 并提取页面信息
static std::vector<std::shared_ptr<BaseAction>> ParseOneNoteHierarchyXML(
	const std::wstring& xml,
	const std::wstring& iconPath,
	int iconIndex,
	int maxResults) {

	std::vector<std::shared_ptr<BaseAction>> result;
	int filteredCount = 0;

	try {
		// 初始化 COM XML
		IXMLDOMDocument2* pXMLDoc = nullptr;
		HRESULT hr = CoCreateInstance(
			__uuidof(DOMDocument60),
			nullptr,
			CLSCTX_INPROC_SERVER,
			__uuidof(IXMLDOMDocument2),
			reinterpret_cast<void**>(&pXMLDoc)
		);

		if (FAILED(hr) || !pXMLDoc) {
			Loge(L"OneNote", L"Failed to create XML DOM document");
			return result;
		}

		// 加载 XML
		VARIANT_BOOL isSuccessful;
		BSTR bstrXml = SysAllocString(xml.c_str());
		hr = pXMLDoc->loadXML(bstrXml, &isSuccessful);
		SysFreeString(bstrXml);

		if (FAILED(hr) || isSuccessful == VARIANT_FALSE) {
			Loge(L"OneNote", L"Failed to load XML");
			pXMLDoc->Release();
			return result;
		}

		// 设置命名空间
		BSTR bstrNamespace = SysAllocString(L"xmlns:one='http://schemas.microsoft.com/office/onenote/2013/onenote'");
		hr = pXMLDoc->setProperty(_bstr_t(L"SelectionNamespaces"), _variant_t(bstrNamespace));
		SysFreeString(bstrNamespace);

		if (FAILED(hr)) {
			ConsolePrintln(L"OneNote", L"Warning: Failed to set namespace property");
		}

		// 查询所有 Page 节点
		IXMLDOMNodeList* pPageNodes = nullptr;
		BSTR bstrQuery = SysAllocString(L"//one:Page");
		hr = pXMLDoc->selectNodes(bstrQuery, &pPageNodes);
		SysFreeString(bstrQuery);

		if (FAILED(hr) || !pPageNodes) {
			pXMLDoc->Release();
			return result;
		}

		long nodeCount = 0;
		pPageNodes->get_length(&nodeCount);

		ConsolePrintln(L"OneNote", L"Found " + std::to_wstring(nodeCount) + L" total pages in XML");

		for (long i = 0; i < nodeCount; ++i) {
			IXMLDOMNode* pPageNode = nullptr;
			hr = pPageNodes->get_item(i, &pPageNode);

			if (SUCCEEDED(hr) && pPageNode) {
				// 获取属性
				IXMLDOMNamedNodeMap* pAttributes = nullptr;
				hr = pPageNode->get_attributes(&pAttributes);

				// 检查是否在回收站中
				bool isInRecycleBin = false;
				if (SUCCEEDED(hr) && pAttributes) {
					IXMLDOMNode* pRecycleBinNode = nullptr;
					BSTR bstrRecycleBin = SysAllocString(L"isInRecycleBin");
					pAttributes->getNamedItem(bstrRecycleBin, &pRecycleBinNode);
					SysFreeString(bstrRecycleBin);

					if (pRecycleBinNode) {
						VARIANT varRecycleBin;
						pRecycleBinNode->get_nodeValue(&varRecycleBin);
						if (varRecycleBin.vt == VT_BSTR) {
							std::wstring recycleBinValue = varRecycleBin.bstrVal;
							isInRecycleBin = (recycleBinValue == L"true");
						}
						VariantClear(&varRecycleBin);
						pRecycleBinNode->Release();
					}
				}

				// 跳过回收站中的页面
				if (isInRecycleBin) {
					filteredCount++;
					if (pAttributes) pAttributes->Release();
					pPageNode->Release();
					continue;
				}

				auto action = std::make_shared<OneNoteAction>();

				if (SUCCEEDED(hr) && pAttributes) {
					// ID
					IXMLDOMNode* pIdNode = nullptr;
					BSTR bstrId = SysAllocString(L"ID");
					pAttributes->getNamedItem(bstrId, &pIdNode);
					SysFreeString(bstrId);

					if (pIdNode) {
						VARIANT varId;
						pIdNode->get_nodeValue(&varId);
						if (varId.vt == VT_BSTR) {
							action->pageId = varId.bstrVal;
						}
						VariantClear(&varId);
						pIdNode->Release();
					}

					// name (页面名称)
					IXMLDOMNode* pNameNode = nullptr;
					BSTR bstrName = SysAllocString(L"name");
					pAttributes->getNamedItem(bstrName, &pNameNode);
					SysFreeString(bstrName);

					if (pNameNode) {
						VARIANT varName;
						pNameNode->get_nodeValue(&varName);
						if (varName.vt == VT_BSTR) {
							action->pageName = varName.bstrVal;
							action->title = varName.bstrVal;
						}
						VariantClear(&varName);
						pNameNode->Release();
					}

					pAttributes->Release();
				}

				// 获取父级 Section 和 Notebook 节点
				IXMLDOMNode* pParentNode = nullptr;
				pPageNode->get_parentNode(&pParentNode);

				if (pParentNode) {
					// Section name
					IXMLDOMNamedNodeMap* pSectionAttrs = nullptr;
					pParentNode->get_attributes(&pSectionAttrs);

					if (pSectionAttrs) {
						IXMLDOMNode* pSectionNameNode = nullptr;
						BSTR bstrSectionName = SysAllocString(L"name");
						pSectionAttrs->getNamedItem(bstrSectionName, &pSectionNameNode);
						SysFreeString(bstrSectionName);

						if (pSectionNameNode) {
							VARIANT varSectionName;
							pSectionNameNode->get_nodeValue(&varSectionName);
							if (varSectionName.vt == VT_BSTR) {
								action->sectionName = varSectionName.bstrVal;
							}
							VariantClear(&varSectionName);
							pSectionNameNode->Release();
						}

						pSectionAttrs->Release();
					}

					// Notebook name (再上一级)
					IXMLDOMNode* pNotebookNode = nullptr;
					pParentNode->get_parentNode(&pNotebookNode);

					if (pNotebookNode) {
						IXMLDOMNamedNodeMap* pNotebookAttrs = nullptr;
						pNotebookNode->get_attributes(&pNotebookAttrs);

						if (pNotebookAttrs) {
							IXMLDOMNode* pNotebookNameNode = nullptr;
							BSTR bstrNotebookName = SysAllocString(L"name");
							pNotebookAttrs->getNamedItem(bstrNotebookName, &pNotebookNameNode);
							SysFreeString(bstrNotebookName);

							if (pNotebookNameNode) {
								VARIANT varNotebookName;
								pNotebookNameNode->get_nodeValue(&varNotebookName);
								if (varNotebookName.vt == VT_BSTR) {
									action->notebookName = varNotebookName.bstrVal;
								}
								VariantClear(&varNotebookName);
								pNotebookNameNode->Release();
							}

							pNotebookAttrs->Release();
						}

						pNotebookNode->Release();
					}

					pParentNode->Release();
				}

				// 设置副标题
				action->subTitle = action->notebookName;
				if (!action->sectionName.empty()) {
					action->subTitle += L"\\" + action->sectionName;
				}

				// 设置图标
				action->iconFilePath = iconPath;
				action->iconFilePathIndex = iconIndex;
				action->matchText = action->title;
				result.push_back(action);

				// 检查是否达到最大数量
				if (maxResults > 0 && static_cast<int>(result.size()) >= maxResults) {
					ConsolePrintln(L"OneNote", L"Reached max results limit: " + std::to_wstring(maxResults));
					pPageNode->Release();
					break;
				}

				pPageNode->Release();
			}
		}

		pPageNodes->Release();
		pXMLDoc->Release();

		if (filteredCount > 0) {
			ConsolePrintln(L"OneNote", L"Filtered out " + std::to_wstring(filteredCount) +
				L" pages from recycle bin");
		}

	} catch (const std::exception& e) {
		Loge(L"OneNote", L"Exception in ParseOneNoteHierarchyXML: ", e.what());
	}

	return result;
}

// 获取所有 OneNote 页面
static std::vector<std::shared_ptr<BaseAction>> GetAllOneNotePages(const std::wstring& iconPath) {
	std::vector<std::shared_ptr<BaseAction>> result;

	try {
		OneNoteHelper oneNote;

		if (!oneNote.IsInitialized()) {
			ConsolePrintln(L"OneNote", L"OneNote is not available");
			return result;
		}

		// 获取所有页面的 XML
		std::wstring xml = oneNote.GetHierarchy(L"", hsPages);

		if (xml.empty()) {
			ConsolePrintln(L"OneNote", L"Failed to get OneNote hierarchy");
			return result;
		}

		// 解析 XML
		int iconIndex = GetSysImageIndex(iconPath);
		result = ParseOneNoteHierarchyXML(xml, iconPath, iconIndex);

		ConsolePrintln(L"OneNote", L"Loaded " + std::to_wstring(result.size()) + L" OneNote pages");

	} catch (const std::exception& e) {
		Loge(L"OneNote", L"Exception in GetAllOneNotePages: ", e.what());
	}

	return result;
}

// 打开 OneNote 页面
static bool OpenOneNotePage(const std::wstring& pageId) {
	try {
		OneNoteHelper oneNote;

		if (!oneNote.IsInitialized()) {
			return false;
		}

		return oneNote.NavigateToPage(pageId);

	} catch (const std::exception& e) {
		Loge(L"OneNote", L"Exception in OpenOneNotePage: ", e.what());
		return false;
	}
}

// 显示 OneNote 窗口
static void ShowOneNoteWindow() {
	HWND hwnd = FindWindowW(L"Framework::CFrame", nullptr);
	if (hwnd) {
		if (IsIconic(hwnd)) {
			ShowWindow(hwnd, SW_RESTORE);
		}
		SetForegroundWindow(hwnd);
	}
}
