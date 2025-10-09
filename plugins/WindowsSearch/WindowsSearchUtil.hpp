#pragma once

#include "WindowsSearchAction.hpp"
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <searchapi.h>
#include <oledb.h>

#include <msdasc.h>
#pragma comment(lib, "oledb.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "ole32.lib")

// CLSID 和 IID 定义
#ifndef WINDOWS_SEARCH_CLSIDS_DEFINED
#define WINDOWS_SEARCH_CLSIDS_DEFINED

// Windows Search Manager CLSID and IID
static const CLSID CLSID_CSearchManager = {0x7D096C5F, 0xAC08, 0x4F1F, {0xBE, 0xB7, 0x5C, 0x22, 0xC5, 0x17, 0xCE, 0x39}};
static const IID IID_ISearchManager = {0xAB310581, 0xAC80, 0x11D1, {0x8D, 0xF3, 0x00, 0xC0, 0x4F, 0xB6, 0xEF, 0x69}};

#endif

#include "util/BitmapUtil.hpp"
#include "util/LogUtil.hpp"

// Windows Search API 搜索帮助类
class WindowsSearchAPI {
private:
	// 修改查询帮助器，添加文件模式和排除模式
	static void ModifyQueryHelper(ISearchQueryHelper* queryHelper, const std::wstring& pattern,
								const std::vector<std::wstring>& excludedPatterns, bool displayHiddenFiles) {
		if (!queryHelper) return;

		try {
			// ❌ 不要使用 get_QueryWhereRestrictions，它可能返回无效数据
			// 像 C# 版本一样，重新构建完整的 WHERE 子句
			// ⚠️ 必须以 "AND" 开头，与 C# 版本保持一致（第 140 行）
			std::wstring whereClause = L"AND scope='file:'";

			if (!displayHiddenFiles) {
				// 排除隐藏文件 (FILE_ATTRIBUTE_HIDDEN = 0x2)
				whereClause += L" AND System.FileAttributes <> SOME BITWISE 2";
			}

			// 转换文件模式（如果不是 '*'）
			if (pattern != L"*") {
				std::wstring modifiedPattern = pattern;
				// 替换通配符: * -> %, ? -> _
				std::wstring result;
				result.reserve(modifiedPattern.length());

				for (wchar_t ch : modifiedPattern) {
					if (ch == L'*') {
						result += L'%';
					} else if (ch == L'?') {
						result += L'_';
					} else {
						result += ch;
					}
				}
				modifiedPattern = result;

				if (modifiedPattern.find(L'%') != std::wstring::npos || modifiedPattern.find(L'_') != std::wstring::npos) {
					whereClause += L" AND System.FileName LIKE '" + modifiedPattern + L"' ";
				} else {
					// 如果没有通配符，使用 Contains 更快（使用索引）
					whereClause += L" AND Contains(System.FileName, '" + modifiedPattern + L"') ";
				}
			}

			// 添加排除模式
			for (const auto& excludedPattern : excludedPatterns) {
				if (excludedPattern.empty()) continue;

				// 第一步：将反斜杠替换为正斜杠
				std::wstring exPattern;
				exPattern.reserve(excludedPattern.length());
				for (wchar_t ch : excludedPattern) {
					if (ch == L'\\') {
						exPattern += L'/';
					} else {
						exPattern += ch;
					}
				}

				if (exPattern.find(L'*') != std::wstring::npos || exPattern.find(L'?') != std::wstring::npos) {
					// 转义 SQL LIKE 特殊字符并替换通配符
					std::wstring result;
					result.reserve(exPattern.length() * 2);

					for (wchar_t ch : exPattern) {
						if (ch == L'%') {
							result += L"[%]";
						} else if (ch == L'_') {
							result += L"[_]";
						} else if (ch == L'*') {
							result += L'%';
						} else if (ch == L'?') {
							result += L'_';
						} else {
							result += ch;
						}
					}

					whereClause += L" AND System.ItemUrl NOT LIKE '%" + result + L"%' ";
				} else {
					whereClause += L" AND NOT Contains(System.ItemUrl, '" + exPattern + L"') ";
				}
			}

			// 打印调试信息
			ConsolePrintln(L"WindowsSearch", L"WHERE clause to set: " + whereClause);

			// 安全地设置新的 WHERE 子句
			BSTR bstrWhereClause = SysAllocString(whereClause.c_str());
			if (bstrWhereClause != nullptr) {
				HRESULT hr = queryHelper->put_QueryWhereRestrictions(bstrWhereClause);
				// ✅ put 会内部复制 BSTR，所以必须立即释放
				SysFreeString(bstrWhereClause);

				if (FAILED(hr)) {
					wchar_t errorMsg[256];
					swprintf_s(errorMsg, 256, L"Failed to set query where restrictions, HRESULT: 0x%08X", hr);
					Loge(L"WindowsSearch", errorMsg);
				} else {
					ConsolePrintln(L"WindowsSearch", L"Successfully set WHERE clause");
				}
			} else {
				Loge(L"WindowsSearch", L"Failed to allocate BSTR for where clause");
			}
		} catch (const std::exception& e) {
			Loge(L"WindowsSearch", L"Error modifying query helper: ", e.what());
		}
	}

	// 初始化查询帮助器
	static bool InitQueryHelper(ISearchQueryHelper** ppQueryHelper, ISearchManager* searchManager, int maxCount, bool displayHiddenFiles) {
		if (!searchManager || !ppQueryHelper) return false;
		*ppQueryHelper = nullptr; // 确保在开始时将其设为 null

		try {
			// 获取 SystemIndex 目录管理器
			ISearchCatalogManager* catalogManager = nullptr;
			HRESULT hr = searchManager->GetCatalog(L"SystemIndex", &catalogManager);
			if (FAILED(hr) || !catalogManager) {
				Loge(L"WindowsSearch", L"Failed to get catalog manager");
				return false;
			}

			// 获取查询帮助器
			hr = catalogManager->GetQueryHelper(ppQueryHelper);
			catalogManager->Release(); // catalogManager 不再需要

			if (FAILED(hr) || !*ppQueryHelper) {
				Loge(L"WindowsSearch", L"Failed to get query helper");
				if (*ppQueryHelper) {
					(*ppQueryHelper)->Release(); // 释放可能返回的无效指针
					*ppQueryHelper = nullptr;
				}
				return false;
			}

			ISearchQueryHelper* queryHelper = *ppQueryHelper;

			// 设置最大结果数
			// *** 必须检查 HRESULT ***
			hr = queryHelper->put_QueryMaxResults(maxCount);
			if (FAILED(hr)) {
				Loge(L"WindowsSearch", L"Failed to set QueryMaxResults");
				queryHelper->Release();
				*ppQueryHelper = nullptr;
				return false;
			}

			// 设置要查询的列
			BSTR selectColumns = SysAllocString(L"System.ItemUrl, System.FileName, System.FileAttributes");
			hr = queryHelper->put_QuerySelectColumns(selectColumns);
			// ✅ put 会内部复制 BSTR，必须立即释放
			SysFreeString(selectColumns);
			// *** 必须检查 HRESULT ***
			if (FAILED(hr)) {
				Loge(L"WindowsSearch", L"Failed to set QuerySelectColumns");
				queryHelper->Release();
				*ppQueryHelper = nullptr;
				return false;
			}

			// ⚠️ WHERE 子句现在由 ModifyQueryHelper 统一设置
			// 这里不再设置，避免 get/put 导致的内存问题

			// 设置内容属性（用于搜索）
			BSTR contentProperties = SysAllocString(L"System.FileName");
			hr = queryHelper->put_QueryContentProperties(contentProperties);
			// ✅ put 会内部复制 BSTR，必须立即释放
			SysFreeString(contentProperties);
			// *** 必须检查 HRESULT ***
			if (FAILED(hr)) {
				Loge(L"WindowsSearch", L"Failed to set QueryContentProperties");
				queryHelper->Release();
				*ppQueryHelper = nullptr;
				return false;
			}

			// 设置排序顺序
			BSTR sorting = SysAllocString(L"System.DateModified DESC");
			hr = queryHelper->put_QuerySorting(sorting);
			// ✅ put 会内部复制 BSTR，必须立即释放
			SysFreeString(sorting);
			// *** 必须检查 HRESULT ***
			if (FAILED(hr)) {
				Loge(L"WindowsSearch", L"Failed to set QuerySorting");
				queryHelper->Release();
				*ppQueryHelper = nullptr;
				return false;
			}

			// 所有设置均成功
			return true;
		} catch (const std::exception& e) {
			Loge(L"WindowsSearch", L"Exception in InitQueryHelper: ", e.what());
			if (ppQueryHelper && *ppQueryHelper) {
				(*ppQueryHelper)->Release();
				*ppQueryHelper = nullptr;
			}
			return false;
		}
	}

	// 简化的 OLE DB 查询执行
	static std::vector<std::shared_ptr<BaseAction>> ExecuteOleDbQuery(const std::wstring& connectionString, const std::wstring& sqlQuery) {
		std::vector<std::shared_ptr<BaseAction>> results;

		try {
			// 初始化 OLE DB，使用数据初始化器
			IDataInitialize* dataInit = nullptr;
			HRESULT hr = CoCreateInstance(CLSID_MSDAINITIALIZE, nullptr, CLSCTX_INPROC_SERVER, IID_IDataInitialize,
										reinterpret_cast<void**>(&dataInit));

			if (FAILED(hr) || !dataInit) {
				Loge(L"WindowsSearch", L"Failed to create data initializer");
				return results;
			}

			IDBInitialize* dbInit = nullptr;
			// 创建可修改的字符串副本（IDataInitialize 需要非 const 参数）
			std::wstring mutableConnectionString = connectionString;
			hr = dataInit->GetDataSource(nullptr, CLSCTX_INPROC_SERVER, const_cast<LPWSTR>(mutableConnectionString.c_str()),
										IID_IDBInitialize, reinterpret_cast<IUnknown**>(&dbInit));
			dataInit->Release();

			if (FAILED(hr) || !dbInit) {
				Loge(L"WindowsSearch", L"Failed to get data source");
				return results;
			}

			hr = dbInit->Initialize();
			if (FAILED(hr)) {
				dbInit->Release();
				Loge(L"WindowsSearch", L"Failed to initialize database");
				return results;
			}

			// 创建会话
			IDBCreateSession* createSession = nullptr;
			hr = dbInit->QueryInterface(IID_IDBCreateSession, reinterpret_cast<void**>(&createSession));

			if (SUCCEEDED(hr) && createSession) {
				IUnknown* session = nullptr;
				hr = createSession->CreateSession(nullptr, IID_IUnknown, &session);

				if (SUCCEEDED(hr) && session) {
					// 创建命令
					IDBCreateCommand* createCommand = nullptr;
					hr = session->QueryInterface(IID_IDBCreateCommand, reinterpret_cast<void**>(&createCommand));

					if (SUCCEEDED(hr) && createCommand) {
						ICommand* command = nullptr;
						hr = createCommand->CreateCommand(nullptr, IID_ICommand, reinterpret_cast<IUnknown**>(&command));

						if (SUCCEEDED(hr) && command) {
							// 设置命令文本
							ICommandText* commandText = nullptr;
							hr = command->QueryInterface(IID_ICommandText, reinterpret_cast<void**>(&commandText));

							if (SUCCEEDED(hr) && commandText) {
								hr = commandText->SetCommandText(DBGUID_DEFAULT, sqlQuery.c_str());

								if (SUCCEEDED(hr)) {
									// 执行命令
									DBROWCOUNT rowsAffected = 0;
									IRowset* rowset = nullptr;
									hr = command->Execute(nullptr, IID_IRowset, nullptr, &rowsAffected,
														reinterpret_cast<IUnknown**>(&rowset));

									if (SUCCEEDED(hr) && rowset) {
										// 获取列信息
										IColumnsInfo* columnsInfo = nullptr;
										hr = rowset->QueryInterface(IID_IColumnsInfo, reinterpret_cast<void**>(&columnsInfo));

										if (SUCCEEDED(hr) && columnsInfo) {
											DBORDINAL numColumns = 0;
											DBCOLUMNINFO* columnInfo = nullptr;
											OLECHAR* stringBuffer = nullptr;

											hr = columnsInfo->GetColumnInfo(&numColumns, &columnInfo, &stringBuffer);

											if (SUCCEEDED(hr) && numColumns >= 2) {
												// 创建访问器
												IAccessor* accessor = nullptr;
												hr = rowset->QueryInterface(IID_IAccessor, reinterpret_cast<void**>(&accessor));

												if (SUCCEEDED(hr) && accessor) {
													// 定义数据绑定结构
													struct RowData {
														DBSTATUS urlStatus;
														DBLENGTH urlLength;
														WCHAR url[4096];
														DBSTATUS fileNameStatus;
														DBLENGTH fileNameLength;
														WCHAR fileName[1024];
													};

													DBBINDING bindings[2];
													memset(bindings, 0, sizeof(bindings));

													// URL 绑定
													bindings[0].iOrdinal = 1;
													bindings[0].obStatus = offsetof(RowData, urlStatus);
													bindings[0].obLength = offsetof(RowData, urlLength);
													bindings[0].obValue = offsetof(RowData, url);
													bindings[0].pTypeInfo = nullptr;
													bindings[0].pObject = nullptr;
													bindings[0].pBindExt = nullptr;
													bindings[0].dwPart = DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS;
													bindings[0].dwMemOwner = DBMEMOWNER_CLIENTOWNED;
													bindings[0].eParamIO = DBPARAMIO_NOTPARAM;
													bindings[0].cbMaxLen = sizeof(RowData::url);
													bindings[0].dwFlags = 0;
													bindings[0].wType = DBTYPE_WSTR;
													bindings[0].bPrecision = 0;
													bindings[0].bScale = 0;

													// FileName 绑定
													bindings[1].iOrdinal = 2;
													bindings[1].obStatus = offsetof(RowData, fileNameStatus);
													bindings[1].obLength = offsetof(RowData, fileNameLength);
													bindings[1].obValue = offsetof(RowData, fileName);
													bindings[1].pTypeInfo = nullptr;
													bindings[1].pObject = nullptr;
													bindings[1].pBindExt = nullptr;
													bindings[1].dwPart = DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS;
													bindings[1].dwMemOwner = DBMEMOWNER_CLIENTOWNED;
													bindings[1].eParamIO = DBPARAMIO_NOTPARAM;
													bindings[1].cbMaxLen = sizeof(RowData::fileName);
													bindings[1].dwFlags = 0;
													bindings[1].wType = DBTYPE_WSTR;
													bindings[1].bPrecision = 0;
													bindings[1].bScale = 0;

													HACCESSOR hAccessor = DB_NULL_HACCESSOR;
													hr = accessor->CreateAccessor(DBACCESSOR_ROWDATA, 2, bindings, sizeof(RowData),
																				&hAccessor, nullptr);

													if (SUCCEEDED(hr)) {
														// 获取行
														while (true) {
															HROW rowHandle = DB_NULL_HROW;
															HROW* pRowHandle = &rowHandle;
															DBCOUNTITEM numRowsObtained = 0;

															hr = rowset->GetNextRows(DB_NULL_HCHAPTER, 0, 1, &numRowsObtained, &pRowHandle);

															if (FAILED(hr) || numRowsObtained == 0) break;

															RowData rowData;
															memset(&rowData, 0, sizeof(rowData));

															hr = rowset->GetData(rowHandle, hAccessor, &rowData);

															if (SUCCEEDED(hr) && rowData.urlStatus == DBSTATUS_S_OK && rowData.
																fileNameStatus == DBSTATUS_S_OK) {
																// 解析 URL 获取文件路径
																std::wstring url = rowData.url;
																std::wstring fileName = rowData.fileName;

																std::wstring path;
																if (url.find(L"file:///") == 0) {
																	path = url.substr(8); // 移除 "file:///"

																	// URL 解码 '#' 字符
																	size_t pos = 0;
																	while ((pos = path.find(L"%23", pos)) != std::wstring::npos) {
																		path.replace(pos, 3, L"#");
																		pos++;
																	}

																	// 替换正斜杠为反斜杠
																	pos = 0;
																	while ((pos = path.find(L"/", pos)) != std::wstring::npos) {
																		path.replace(pos, 1, L"\\");
																		pos++;
																	}
																} else {
																	path = url;
																}
																if (MyStartsWith2(path, L"file:")) {
																	path = path.substr(5);
																}

																// 创建 Action
																auto action = std::make_shared<WindowsSearchAction>();
																action->path = path;
																action->fileName = fileName;
																action->title = fileName;
																action->subTitle = path;
																action->iconFilePath = path;
																action->iconFilePathIndex = GetSysImageIndex(path);

																// 设置匹配文本
																// try {
																// 	if (isMatchTextUrl) {
																// 		action->matchText = m_host->GetTheProcessedMatchingText(fileName) +
																// 			path;
																// 	} else {
																// 		action->matchText = m_host->GetTheProcessedMatchingText(fileName);
																// 	}
																// } catch (...) {
																// 	action->matchText = fileName;
																// }

																results.push_back(action);
															}

															rowset->ReleaseRows(1, &rowHandle, nullptr, nullptr, nullptr);
														}

														accessor->ReleaseAccessor(hAccessor, nullptr);
													}

													accessor->Release();
												}

												CoTaskMemFree(columnInfo);
												CoTaskMemFree(stringBuffer);
											}

											columnsInfo->Release();
										}

										rowset->Release();
									}
								}

								commandText->Release();
							}

							command->Release();
						}

						createCommand->Release();
					}

					session->Release();
				}

				createSession->Release();
			}

			dbInit->Release();
		} catch (const std::exception& e) {
			Loge(L"WindowsSearch", L"Exception in ExecuteOleDbQuery: ", e.what());
		}

		return results;
	}

public:
	// 执行搜索查询
	static std::vector<std::shared_ptr<BaseAction>> Search(const std::wstring& keyword, int maxCount = 30, bool displayHiddenFiles = false,
															const std::wstring& pattern = L"*",
															const std::vector<std::wstring>& excludedPatterns = {}) {
		std::vector<std::shared_ptr<BaseAction>> results;

		if (keyword.empty()) {
			return results;
		}

		try {
			// 初始化 COM
			HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
			bool comInitialized = SUCCEEDED(hr);

			// 如果已经初始化（S_FALSE），也是成功的
			if (hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
				comInitialized = false; // 不需要在退出时调用 CoUninitialize
			}

			// 创建搜索管理器 - 尝试不同的 CLSCTX 选项
			ISearchManager* searchManager = nullptr;
			hr = CoCreateInstance(CLSID_CSearchManager, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_ISearchManager,
								reinterpret_cast<void**>(&searchManager));

			// 如果失败，记录详细错误信息
			if (FAILED(hr)) {
				wchar_t errorMsg[512];
				const wchar_t* errorDesc = L"Unknown error";

				switch (hr) {
				case REGDB_E_CLASSNOTREG: errorDesc = L"Class not registered (REGDB_E_CLASSNOTREG)";
					break;
				case CLASS_E_NOAGGREGATION: errorDesc = L"No aggregation (CLASS_E_NOAGGREGATION)";
					break;
				case E_NOINTERFACE: errorDesc = L"No interface (E_NOINTERFACE)";
					break;
				case E_POINTER: errorDesc = L"Invalid pointer (E_POINTER)";
					break;
				case CO_E_NOTINITIALIZED: errorDesc = L"COM not initialized (CO_E_NOTINITIALIZED)";
					break;
				case RPC_E_CHANGED_MODE: errorDesc = L"COM mode mismatch (RPC_E_CHANGED_MODE)";
					break;
				default: break;
				}

				swprintf_s(errorMsg, 512, L"Failed to create search manager. HRESULT: 0x%08X - %s", hr, errorDesc);
				Loge(L"WindowsSearch", errorMsg);

				// 检查 Windows Search 服务是否运行
				SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
				if (scManager) {
					SC_HANDLE service = OpenService(scManager, L"WSearch", SERVICE_QUERY_STATUS);
					if (service) {
						SERVICE_STATUS status;
						if (QueryServiceStatus(service, &status)) {
							if (status.dwCurrentState != SERVICE_RUNNING) {
								Loge(L"WindowsSearch", L"Windows Search service is not running. Please start the service.");
							}
						}
						CloseServiceHandle(service);
					} else {
						Loge(L"WindowsSearch", L"Windows Search service not found.");
					}
					CloseServiceHandle(scManager);
				}

				if (comInitialized) CoUninitialize();
				const auto action = std::make_shared<WindowsSearchAction>();
				action->title = L"WindowsSearch 服务运行异常";
				action->subTitle = L"转到 Windows Search 设置进行修复";
				action->path = L"OpenSettingsAction";
				// 使用系统设置图标
				action->iconFilePath = L"shell32.dll";
				action->iconFilePathIndex = 21; // 设置图标索引
				results.push_back(action);
				return results;
			}

			if (!searchManager) {
				Loge(L"WindowsSearch", L"Search manager is null");
				if (comInitialized) CoUninitialize();
				return results;
			}

			// 初始化查询帮助器
			ISearchQueryHelper* queryHelper = nullptr;
			if (!InitQueryHelper(&queryHelper, searchManager, maxCount, displayHiddenFiles)) {
				searchManager->Release();
				if (comInitialized) CoUninitialize();
				Loge(L"WindowsSearch", L"InitQueryHelper fail: ");
				return results;
			}

			// 修改查询帮助器（添加过滤条件）
			ModifyQueryHelper(queryHelper, pattern, excludedPatterns, displayHiddenFiles);

			// 从用户查询生成 SQL
			ConsolePrintln(L"WindowsSearch", L"Input keyword: " + keyword);

			BSTR sqlQuery = nullptr;
			BSTR bstrKeyword = SysAllocString(keyword.c_str());
			hr = queryHelper->GenerateSQLFromUserQuery(bstrKeyword, &sqlQuery);
			SysFreeString(bstrKeyword);

			if (FAILED(hr)) {
				wchar_t errorMsg[256];
				swprintf_s(errorMsg, 256, L"GenerateSQLFromUserQuery failed, HRESULT: 0x%08X", hr);
				Loge(L"WindowsSearch", errorMsg);
				queryHelper->Release();
				searchManager->Release();
				if (comInitialized) CoUninitialize();
				return results;
			}

			// ⚠️ 验证 BSTR 的有效性
			if (!sqlQuery) {
				Loge(L"WindowsSearch", L"sqlQuery is nullptr");
				queryHelper->Release();
				searchManager->Release();
				if (comInitialized) CoUninitialize();
				return results;
			}

			// ✅ 使用 wcslen 而不是 SysStringLen，因为某些实现返回的可能不是标准 BSTR
			size_t sqlLen = 0;
			try {
				sqlLen = wcslen(sqlQuery);
			} catch (...) {
				Loge(L"WindowsSearch", L"wcslen crashed - invalid pointer");
				// 不要调用 SysFreeString，因为这可能不是有效的 BSTR
				queryHelper->Release();
				searchManager->Release();
				if (comInitialized) CoUninitialize();
				return results;
			}

			if (sqlLen == 0) {
				Loge(L"WindowsSearch", L"sqlQuery length is 0");
				SysFreeString(sqlQuery);
				queryHelper->Release();
				searchManager->Release();
				if (comInitialized) CoUninitialize();
				return results;
			}

			// 获取连接字符串
			BSTR connectionString = nullptr;
			queryHelper->get_ConnectionString(&connectionString);

			// ✅ 直接从 wchar_t* 构造 std::wstring
			std::wstring sql(sqlQuery);
			ConsolePrintln(L"WindowsSearch", L"SQL Query: " + sql);

			// 执行 OLE DB 查询
			results = ExecuteOleDbQuery(connectionString, sqlQuery);

			// ⚠️ 重要：不要释放这些 BSTR！
			// GenerateSQLFromUserQuery 和 get_ConnectionString 返回的不是标准 BSTR，
			// 而是 COM 对象内部管理的字符串指针（类似于返回内部缓冲区）。
			// 这些指针不是通过 SysAllocString 分配的，所以不能用 SysFreeString 释放。
			// 调用 SysFreeString 会导致崩溃（访问违规）。
			// 这些内存会在 queryHelper->Release() 时由 COM 对象自动清理。
			// SysFreeString(sqlQuery);  // ❌ 崩溃！
			// SysFreeString(connectionString);  // ❌ 崩溃！

			queryHelper->Release();
			searchManager->Release();

			if (comInitialized) CoUninitialize();

			ConsolePrintln(L"WindowsSearch", L"Found " + std::to_wstring(results.size()) + L" results");
		} catch (const std::exception& e) {
			Loge(L"WindowsSearch", L"Exception in Search: ", e.what());
		}

		return results;
	}
};
