# WindowsSearch Plugin - C++ 实现文档

## 概述

本插件使用 Windows Search API (COM 接口) 实现文件搜索功能。这是从 C# 版本移植到 C++ 的实现，遇到了许多 COM/BSTR 内存管理的陷阱。

## 核心架构

```
WindowsSearchPlugin.cpp (插件入口)
    └─> WindowsSearchAPI::Search() (搜索逻辑)
        ├─> InitQueryHelper() (初始化查询助手)
        ├─> ModifyQueryHelper() (设置 WHERE 子句)
        ├─> GenerateSQLFromUserQuery() (生成 SQL)
        └─> ExecuteOleDbQuery() (执行查询)
```

## 关键问题与解决方案

### 🔥 问题 1: `get_QueryWhereRestrictions` 返回无效 BSTR

#### 现象
```cpp
BSTR restrictions = nullptr;
hr = queryHelper->get_QueryWhereRestrictions(&restrictions);
UINT len = SysStringLen(restrictions);  // ❌ len = 1207980416 (垃圾值)
whereClause.assign(restrictions, len);   // 💥 崩溃！
```

#### 根本原因
- COM 对象的 `get` 方法返回的 BSTR 可能是无效的或未初始化的
- 在 `InitQueryHelper` 中设置的值无法通过 `get` 可靠地获取回来

#### 解决方案
**不使用 `get`，而是像 C# 版本一样重建完整的 WHERE 子句**：

```cpp
// ❌ 错误做法
BSTR restrictions = nullptr;
queryHelper->get_QueryWhereRestrictions(&restrictions);
std::wstring whereClause(restrictions, SysStringLen(restrictions));

// ✅ 正确做法
std::wstring whereClause = L"AND scope='file:'";
if (!displayHiddenFiles) {
    whereClause += L" AND System.FileAttributes <> SOME BITWISE 2";
}
// ... 追加其他条件 ...
queryHelper->put_QueryWhereRestrictions(SysAllocString(whereClause.c_str()));
```

#### C# 对比
```csharp
// C# 版本直接用 += 追加，运行时自动处理
queryHelper.QueryWhereRestrictions = "AND scope='file:'";
queryHelper.QueryWhereRestrictions += " AND System.FileName LIKE '" + pattern + "' ";
```

---

### 🔥 问题 3: BSTR 到 std::wstring 转换错误

#### 现象
```cpp
const std::wstring sql = std::wstring(sqlQuery, sqlQuery + SysStringLen(sqlQuery));
// 💥 访问违规异常 0xc0000005
```

#### 根本原因
- `BSTR` 是 `wchar_t*` 指针，不是迭代器
- 用指针算术 `sqlQuery + len` 构造 wstring 是未定义行为

#### 解决方案
```cpp
// ❌ 错误：使用迭代器范围构造
std::wstring sql(sqlQuery, sqlQuery + SysStringLen(sqlQuery));

// ✅ 正确：使用长度构造或直接构造
std::wstring sql(sqlQuery, SysStringLen(sqlQuery));
// 或
std::wstring sql(sqlQuery);  // 依赖 NULL 结尾
```

---

### 🔥 问题 4: `SysStringLen` 返回垃圾值

#### 现象
```cpp
UINT len = SysStringLen(sqlQuery);  // len = 134474570 (垃圾值)
std::wstring sql(sqlQuery, len);    // 💥 崩溃！
```

#### 根本原因
**Windows Search API 返回的不是标准 BSTR！**

标准 BSTR 布局：
```
[4字节长度] [字符串内容] [\0]
             ^
             BSTR 指针
```

Windows Search API 实际返回：
```
[字符串内容] [\0]
^
普通 wchar_t* 指针
```

`SysStringLen` 会读取指针前 4 个字节作为长度，导致读取到垃圾数据。

#### 解决方案
**使用 `wcslen` 而不是 `SysStringLen`**：

```cpp
// ❌ 错误：假设是标准 BSTR
UINT len = SysStringLen(sqlQuery);  // 读取垃圾数据

// ✅ 正确：当作普通字符串
size_t len = wcslen(sqlQuery);  // 读取到 \0
// 或直接构造
std::wstring sql(sqlQuery);
```

---

### 🔥 问题 5: `SysFreeString` 导致崩溃

#### 现象
```cpp
BSTR sqlQuery = nullptr;
queryHelper->GenerateSQLFromUserQuery(keyword, &sqlQuery);
// ... 使用 sqlQuery ...
SysFreeString(sqlQuery);  // 💥 崩溃！访问违规
```

#### 根本原因
`GenerateSQLFromUserQuery` 和 `get_ConnectionString` 返回的**不是新分配的 BSTR**，而是 COM 对象内部管理的字符串指针。

#### BSTR 的两种情况

##### 情况 1: 调用者负责释放
```cpp
BSTR str = SysAllocString(L"test");  // 你分配的
// ... 使用 ...
SysFreeString(str);  // ✅ 必须释放
```

##### 情况 2: COM 对象管理（Windows Search API 的情况）
```cpp
BSTR str = nullptr;
obj->GenerateSQLFromUserQuery(keyword, &str);  // 返回内部指针
// ... 使用 ...
// ❌ 不能调用 SysFreeString(str) - 会崩溃！
obj->Release();  // ✅ 对象释放时自动清理
```

#### 解决方案
**不要释放这些 BSTR**：

```cpp
BSTR sqlQuery = nullptr;
BSTR connectionString = nullptr;

queryHelper->GenerateSQLFromUserQuery(keyword, &sqlQuery);
queryHelper->get_ConnectionString(&connectionString);

// 使用这些字符串...
std::wstring sql(sqlQuery);

// ⚠️ 不要释放！
// SysFreeString(sqlQuery);        // ❌ 崩溃
// SysFreeString(connectionString); // ❌ 崩溃

queryHelper->Release();  // ✅ 这里会自动清理所有内部资源
```

#### C# 对比
```csharp
// C# 互操作层自动检测并处理
string sql = queryHelper.GenerateSQLFromUserQuery(keyword);
// 运行时自动：
// 1. 检测 BSTR 类型
// 2. 复制到托管字符串
// 3. 智能决定是否需要释放
```

---

### 🔥 问题 6: BSTR 内存管理规则混乱

#### 哪些 BSTR 需要释放？

| API 调用 | 需要 SysFreeString? | 原因 |
|---------|-------------------|------|
| `SysAllocString(L"text")` | ✅ 是 | 你手动分配的 |
| `queryHelper->put_QuerySelectColumns(bstr)` | ✅ 是（调用后） | `put` 会内部复制，你的原始 BSTR 仍需释放 |
| `queryHelper->GenerateSQLFromUserQuery(&bstr)` | ❌ 否 | 返回内部指针 |
| `queryHelper->get_ConnectionString(&bstr)` | ❌ 否 | 返回内部指针 |
| `queryHelper->get_QueryWhereRestrictions(&bstr)` | ⚠️ 可能 | 不可靠，避免使用 |

#### 正确的模式

```cpp
// 模式 1: put 操作
BSTR value = SysAllocString(L"some value");
hr = queryHelper->put_SomeProperty(value);
SysFreeString(value);  // ✅ put 会复制，立即释放

// 模式 2: GenerateSQLFromUserQuery
BSTR sqlQuery = nullptr;
hr = queryHelper->GenerateSQLFromUserQuery(keyword, &sqlQuery);
std::wstring sql(sqlQuery);  // 复制到 wstring
// ❌ 不要 SysFreeString(sqlQuery)

// 模式 3: get_ConnectionString
BSTR connectionString = nullptr;
hr = queryHelper->get_ConnectionString(&connectionString);
// 使用 connectionString...
// ❌ 不要 SysFreeString(connectionString)
```

---

## 完整的正确实现模板

```cpp
// 1. 初始化 QueryHelper（不设置 WHERE 子句）
bool InitQueryHelper(ISearchQueryHelper** ppQueryHelper, ISearchManager* searchManager,
                     int maxCount, bool displayHiddenFiles) {
    // ... 获取 catalogManager 和 queryHelper ...

    // 设置列
    BSTR selectColumns = SysAllocString(L"System.ItemUrl, System.FileName, System.FileAttributes");
    hr = queryHelper->put_QuerySelectColumns(selectColumns);
    SysFreeString(selectColumns);  // ✅ put 会复制，立即释放

    // ⚠️ 不在这里设置 WHERE 子句！由 ModifyQueryHelper 统一处理

    // 设置内容属性
    BSTR contentProperties = SysAllocString(L"System.FileName");
    hr = queryHelper->put_QueryContentProperties(contentProperties);
    SysFreeString(contentProperties);  // ✅ 立即释放

    // 设置排序
    BSTR sorting = SysAllocString(L"System.DateModified DESC");
    hr = queryHelper->put_QuerySorting(sorting);
    SysFreeString(sorting);  // ✅ 立即释放

    return true;
}

// 2. 设置完整的 WHERE 子句
void ModifyQueryHelper(ISearchQueryHelper* queryHelper, const std::wstring& pattern,
                       const std::vector<std::wstring>& excludedPatterns, bool displayHiddenFiles) {
    // ❌ 不要用 get_QueryWhereRestrictions
    // 直接重建完整的 WHERE 子句
    std::wstring whereClause = L"AND scope='file:'";  // ⚠️ 必须以 AND 开头

    if (!displayHiddenFiles) {
        whereClause += L" AND System.FileAttributes <> SOME BITWISE 2";
    }

    if (pattern != L"*") {
        // ... 处理文件模式 ...
        whereClause += L" AND System.FileName LIKE '" + modifiedPattern + L"' ";
    }

    // ... 处理排除模式 ...

    // 设置 WHERE 子句
    BSTR bstrWhereClause = SysAllocString(whereClause.c_str());
    HRESULT hr = queryHelper->put_QueryWhereRestrictions(bstrWhereClause);
    SysFreeString(bstrWhereClause);  // ✅ put 会复制，立即释放
}

// 3. 生成并执行查询
std::vector<Result> Search(const std::wstring& keyword, ...) {
    // ... 初始化 COM 和 searchManager ...

    ISearchQueryHelper* queryHelper = nullptr;
    InitQueryHelper(&queryHelper, searchManager, maxCount, displayHiddenFiles);
    ModifyQueryHelper(queryHelper, pattern, excludedPatterns, displayHiddenFiles);

    // 生成 SQL
    BSTR sqlQuery = nullptr;
    BSTR bstrKeyword = SysAllocString(keyword.c_str());
    hr = queryHelper->GenerateSQLFromUserQuery(bstrKeyword, &sqlQuery);
    SysFreeString(bstrKeyword);  // ✅ 输入参数需要释放

    // 获取连接字符串
    BSTR connectionString = nullptr;
    queryHelper->get_ConnectionString(&connectionString);

    // ✅ 使用 wcslen 或直接构造，不用 SysStringLen
    std::wstring sql(sqlQuery);

    // 执行查询
    results = ExecuteOleDbQuery(connectionString, sqlQuery);

    // ⚠️ 重要：不要释放这些 BSTR！
    // SysFreeString(sqlQuery);        // ❌ 会崩溃
    // SysFreeString(connectionString); // ❌ 会崩溃

    queryHelper->Release();  // ✅ 这里会自动清理所有内部资源
    searchManager->Release();

    return results;
}
```

---

## C# vs C++ 对比总结

| 方面 | C# 实现 | C++ 实现 | 难点 |
|------|---------|---------|------|
| **字符串管理** | 自动（GC） | 手动（BSTR） | ⭐⭐⭐⭐⭐ |
| **内存释放** | 自动 | 需要判断是否释放 | ⭐⭐⭐⭐⭐ |
| **BSTR 类型检测** | 运行时自动 | 手动判断 | ⭐⭐⭐⭐ |
| **错误提示** | 清晰的异常 | 访问违规、垃圾值 | ⭐⭐⭐⭐⭐ |

---

## 最佳实践与注意事项

### ✅ 必须遵守的规则

1. **永远不要使用 `get_QueryWhereRestrictions`**
   - 返回值不可靠
   - 改为重建完整的 WHERE 子句


3. **`put_*` 后立即释放 BSTR**
   ```cpp
   BSTR value = SysAllocString(L"...");
   queryHelper->put_Something(value);
   SysFreeString(value);  // ✅ 必须
   ```

4. **`GenerateSQLFromUserQuery` 和 `get_ConnectionString` 不释放**
   ```cpp
   BSTR sqlQuery = nullptr;
   queryHelper->GenerateSQLFromUserQuery(keyword, &sqlQuery);
   // ❌ SysFreeString(sqlQuery);
   ```

5. **使用 `wcslen` 而不是 `SysStringLen`**
   ```cpp
   size_t len = wcslen(sqlQuery);           // ✅
   UINT len = SysStringLen(sqlQuery);       // ❌ 返回垃圾值
   ```

6. **BSTR 转 wstring 的正确方法**
   ```cpp
   std::wstring str(bstr);                  // ✅ 推荐
   std::wstring str(bstr, wcslen(bstr));    // ✅ 可行
   std::wstring str(bstr, SysStringLen(bstr)); // ❌ 对于 Windows Search API
   ```

### ⚠️ 调试技巧

1. **BSTR 长度异常**
   - 如果 `SysStringLen` 返回超大值（> 1000000），说明不是标准 BSTR
   - 改用 `wcslen`

2. **访问违规异常 0xc0000005**
   - 检查是否错误释放了 COM 对象管理的 BSTR
   - 检查是否用 `SysStringLen` 处理了非标准 BSTR

3. **SQL 语法错误**
   - 打印完整的 SQL 查询语句

4. **内存泄漏检测**
   - 只有 `SysAllocString` 分配的才需要 `SysFreeString`
   - COM 对象返回的内部指针由对象管理

---

## 参考资料

- [Windows Search API 文档](https://learn.microsoft.com/en-us/windows/win32/search/-search-3x-wds-overview)
- [BSTR 内存管理](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/automat/bstr)
- C# 参考实现：`plugins/WindowsSearchCsharp/SearchHelper/WindowsSearchAPI.cs`

---

## 许可证

本插件基于 PowerToys 的 WindowsSearch 插件移植，遵循 MIT 许可证。
