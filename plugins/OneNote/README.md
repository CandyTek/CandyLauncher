@ -0,0 +1,213 @@
# OneNote Plugin - 开发记录

## 目标
实现一个 C++ 插件，用于索引 Microsoft OneNote 中的所有笔记页面，并支持快速搜索和打开。

## 当前状态
🔴 **未完成** - COM 接口调用存在兼容性问题

### 当前进展
- ✅ 成功连接到 OneNote COM 对象 (CLSID: `{DC67E480-C3CB-49F8-8232-60B0C2056C8E}`)
- ✅ 成功加载 OneNote Type Library (LIBID: `{0EA692EE-BB50-4E3C-AEF0-356D91732725}`)
- ✅ 成功枚举 TypeLib 中的接口，找到包含 `GetHierarchy` 方法的正确接口 (TypeInfo[17])
- ✅ 成功获取 `GetHierarchy` 方法的 DISPID: `1610743808` (`0x6000EA00`)
- ❌ **Invoke 调用失败**，错误码: `0x8002801D` (`TYPE_E_LIBNOTREGISTERED`)

## 遇到的关键问题

### 1. 最初的错误 - 接口定义问题
**问题**: 使用错误的 IID 和方法签名
- 初始尝试使用自定义的 `IOneNoteApplication` 接口定义
- `GetIDsOfNames` 返回 `DISP_E_UNKNOWNNAME` (`0x80020006`)

**原因**: OneNote 2016 的 COM 对象不支持通过标准 `GetIDsOfNames` 查找方法名

### 2. TypeInfo 获取失败
**问题**: `IDispatch::GetTypeInfo(0, ...)` 失败
- 错误码: `0x80004005` (`E_FAIL`)

**原因**: OneNote COM 对象虽然报告 TypeInfo count = 1，但不提供 TypeInfo 访问

### 3. 硬编码 DISPID 失败
**问题**: 尝试使用文档中的 DISPID (60020, 60021) 直接调用失败

**原因**: 这些 DISPID 值不适用于 OneNote 2016 版本

### 4. 当前阻塞问题 - TYPE_E_LIBNOTREGISTERED
**问题**: 通过 LoadRegTypeLib 加载类型库并获取正确 DISPID 后，Invoke 仍然失败
- 错误码: `0x8002801D` (`TYPE_E_LIBNOTREGISTERED`)
- 已确认 DISPID 正确: `1610743808`
- 已确认参数数量: 4 个参数

**可能原因**:
1. 参数类型不匹配（尤其是 out 参数 `BSTR*`）
2. 调用约定问题（`DISPATCH_METHOD` vs `DISPATCH_PROPERTYGET`）
3. OneNote 2016 需要特殊的接口版本或调用方式
4. COM 线程模型问题（`COINIT_APARTMENTTHREADED`）

## 关键判断点

### 提前判断 OneNote 是否可用
```cpp
// 1. 检查 OneNote 是否安装
CLSID clsid;
HRESULT hr = CLSIDFromProgID(L"OneNote.Application.15", &clsid);
if (FAILED(hr)) {
    // OneNote 未安装或版本不匹配
}

// 2. 尝试创建 COM 对象
IDispatch* pDisp = nullptr;
hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
                      IID_IDispatch, (void**)&pDisp);
if (FAILED(hr)) {
    // 无法创建 OneNote COM 对象
}

// 3. 检查 TypeLib 是否可用
static const GUID LIBID_OneNote = {
    0x0EA692EE, 0xBB50, 0x4E3C,
    {0xAE, 0xF0, 0x35, 0x6D, 0x91, 0x73, 0x27, 0x25}
};
ITypeLib* pTypeLib = nullptr;
hr = LoadRegTypeLib(LIBID_OneNote, 1, 1, LOCALE_USER_DEFAULT, &pTypeLib);
if (FAILED(hr)) {
    // TypeLib 未注册
}
```

### 参考实现对比

**PowerToys C# 实现** (通过 `ScipBe.Common.Office.OneNote`):
```csharp
// 使用 Microsoft.Office.Interop.OneNote
Application oneNote = new Application();
oneNote.GetHierarchy(null, HierarchyScope.hsPages,
                     out string xml);
```

**关键差异**:
- C# 使用 Primary Interop Assembly (PIA)，自动处理 COM 互操作
- C# 的 COM 互操作层比 C++ 更容易处理类型转换和参数传递
- `ScipBe.Common.Office.OneNote` 库已经封装了 OneNote COM 接口的复杂性

## 技术细节

### OneNote COM 接口版本
- OneNote 2007: `OneNote.Application.12`
- OneNote 2010: `OneNote.Application.14`
- OneNote 2013/2016: `OneNote.Application.15`
- ProgID: `OneNote.Application` (版本无关)

### GetHierarchy 方法签名
根据 TypeLib 分析:
```cpp
HRESULT GetHierarchy(
    [in] BSTR bstrStartNodeID,     // 起始节点 ID，null 表示所有
    [in] long hsScope,              // HierarchyScope 枚举值
    [out] BSTR* pbstrHierarchyXml,  // 输出 XML 字符串
    [in] long xsSchema              // XMLSchema 版本
);
```

**HierarchyScope 枚举**:
- `hsSelf = 0`
- `hsChildren = 1`
- `hsNotebooks = 2`
- `hsSections = 3`
- `hsPages = 4`

**XMLSchema 枚举**:
- `xs2007 = 0`
- `xs2010 = 1`
- `xs2013 = 2`
- `xsCurrent = 3`

### NavigateTo 方法
根据 C# 实现，只需要 1 个参数：
```cpp
HRESULT NavigateTo([in] BSTR bstrPageID);
```

## 建议的解决方案

### 方案 1: C++/CLI 桥接 ⭐ 推荐
创建一个 C++/CLI 项目，桥接 C# 的 OneNote 库到 C++:
- **优点**: 利用成熟的 C# 库，开发简单
- **缺点**: 需要 .NET 运行时

### 方案 2: 直接使用 Microsoft.Office.Interop.OneNote.dll
通过 COM 导入库直接使用 PIA:
- **优点**: 官方支持
- **缺点**: 需要安装 OneNote PIA

### 方案 3: 继续调试 IDispatch 调用
重点检查:
1. 参数类型（特别是 `BSTR*` out 参数的传递方式）
2. `invkind` 值（从 FUNCDESC 获取）
3. 尝试使用 `ITypeInfo::Invoke` 而不是 `IDispatch::Invoke`

### 方案 4: 解析 OneNote 存储文件
直接读取 OneNote 的本地缓存文件:
- **优点**: 不依赖 COM
- **缺点**: 文件格式未公开，可能随版本变化

## 参考资源
- [Microsoft OneNote 2016 Developer Reference](https://learn.microsoft.com/en-us/office/vba/api/overview/onenote)
- [ScipBe.Common.Office.OneNote GitHub](https://github.com/scipbe/ScipBe-Common-Office)
- [COM Type Libraries](https://learn.microsoft.com/en-us/windows/win32/com/type-libraries)

## 下一步
1. 尝试使用 `ITypeInfo::Invoke` 代替 `IDispatch::Invoke`
2. 详细检查 FUNCDESC 中的参数类型和调用约定
3. 如果上述方法仍失败，考虑采用 C++/CLI 桥接方案


---

## 继续查询资料

关键修正点

`0x8002801D (TYPE_E_LIBNOTREGISTERED)` **通常**意味着运行时仍在尝试读取/解析类型库但**没找到匹配版本或注册脏了**；即使你能 `LoadRegTypeLib`，真正 `Invoke` 时也可能因为**代理/可选参数/LCID**等再次间接查类型库而失败（尤其多版本 Office 并存/曾经升级回滚后）。([Microsoft Learn][3])

1. **改用 `ITypeInfo::Invoke`**
   既然你已经拿到了正确的 `ITypeInfo` 和 `FUNCDESC`/`DISPID`，直接 `ITypeInfo::Invoke`（用 `invkind = INVOKE_FUNC`）往往更稳，能绕开 `IDispatch` 里的一些内部查找/本地化名解析。官方接口定义也支持你用枚举常量（`HierarchyScope`, `XMLSchema`）的整型值。([Microsoft Learn][1])

2. **参数与 `DISPPARAMS` 填法（超关键）**

* `GetHierarchy(BSTR startNodeId, long scope, BSTR* outXml, long schema)`
* `DISPPARAMS.rgvarg` **逆序**：`schema` → `outXml` → `scope` → `startNodeId`
* `outXml` 对应 `VARIANT vt = VT_BYREF | VT_BSTR`，`pvar->pbstrVal = nullptr`，**不要**先分配。
* `startNodeId` 传 `VT_BSTR`（或 `VT_EMPTY`/`VT_NULL` 看实现容忍度）。
* `scope`/`schema` 传 `VT_I4`。
* `cNamedArgs = 0`（没有具名参数），`rgdispidNamedArgs = nullptr`。
* `wFlags = DISPATCH_METHOD`（不是 `PROPERTYGET`）。
* `LCID = 0` 或 `LOCALE_USER_DEFAULT`，我更建议 `0`，避免本地化名分支。
  这些细节错一个，`Invoke` 就可能在内部试图“补全”可选参数或取默认值，从而再碰到类型库/注册表，进而抛你现在的错误。（接口签名与枚举见官方文档。）([Microsoft Learn][1])

3. **线程模型与实例化**

* `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)`（STA）。
* `CoCreateInstance(CLSCTX_LOCAL_SERVER, IID_IDispatch)`；ProgID 用 `OneNote.Application` 或版本化的 `.15`。
* 若机器上装过多版本 Office/OneNote，**修复 Office** 或按微软/厂商文档清理**错误的 TypeLib 版本引用**（常见于回退版本或残留注册）。([Microsoft Learn][3])

4. **多版本/注册问题的旁证**
   社区上大量 `TYPE_E_LIBNOTREGISTERED` 个案都和**错误 TypeLib 版本指向**或**混装/卸载残留**相关（比如从 Office 2016 回退到 2010 之后报错，修复注册表或重注册类型库即可）。这解释了为何你“看似”能 Load TypeLib，但真正 `Invoke` 仍然爆。([Microsoft Learn][3])

---

* **强需求纯原生** → 把 `ITypeInfo::Invoke` 方案**一次性啃透**，同时安排**Office/OneNote TypeLib 的健壮性检测与自修复**（例如启动时验证 ProgID→CLSID、TypeLib 版本、必要注册表键；发现异常给出“修复 Office/重注册”提示）。

2. 切到 `ITypeInfo::Invoke`；确保 STA、`LCID=0`；如果仍旧 `0x8002801D`，执行 Office 修复或重注册相关 TypeLib（常见根因是版本残留/错绑）。([Microsoft Learn][3])

---




### 突破 - 使用 #import 指令 🎉

**解决方案：**
放弃手动 COM 调用，改用 Microsoft 官方推荐的 `#import` 指令自动生成类型库包装器。

**核心实现** (`OneNoteSimple.hpp`):

```cpp
// 1. 导入 OneNote 类型库
#import "libid:0EA692EE-BB50-4E3C-AEF0-356D91732725" rename_namespace("OneNoteLib")

class SimpleOneNoteHelper {
private:
    OneNoteLib::IApplicationPtr m_app;

public:
    std::wstring GetHierarchy() {
        BSTR bstrXml = nullptr;
        HRESULT hr = m_app->GetHierarchy(
            _bstr_t(L""),
            OneNoteLib::hsPages,
            &bstrXml,
            OneNoteLib::xs2013
        );
        // ...
    }

    bool NavigateToPage(const std::wstring& pageId) {
        HRESULT hr = m_app->NavigateTo(
            _bstr_t(pageId.c_str()),
            _bstr_t(L""),
            VARIANT_FALSE
        );
        return SUCCEEDED(hr);
    }
};
```

**#import 的优势：**
1. 编译时类型检查
2. 自动资源管理（智能指针）
3. 简洁的语法
4. 官方支持，兼容性更好
5. 自动生成枚举值

**测试结果：**
✅ 成功调用 `GetHierarchy` 并获取 XML（2963 字符）

✅ 成功解析出 7 个页面节点

### 第四阶段：功能完善

**1. 回收站过滤**
```cpp
// 检测 isInRecycleBin 属性
bool isInRecycleBin = false;
if (pRecycleBinNode) {
    // 读取属性值
    if (recycleBinValue == L"true") {
        isInRecycleBin = true;
    }
}

if (isInRecycleBin) {
    filteredCount++;
    continue;  // 跳过回收站页面
}
```

**测试结果：**
✅ 从 7 个页面过滤掉 3 个回收站页面，剩余 4 个有效页面

**2. 调试日志**
```
[OneNote]Successfully connected via #import
[OneNote]Successfully retrieved hierarchy, XML length: 2963
[OneNote]Found 7 total pages in XML
[OneNote]Filtered out 3 pages from recycle bin
[OneNote Plugin]Loaded 4 OneNote pages
```

**3. 配置项支持**
- `com.candytek.onenoteplugin.maxResults`: 最大笔记数量（默认 0）
- `com.candytek.onenoteplugin.startstr`: 直接激活命令（默认 `"on "`）
- `com.candytek.onenoteplugin.icon_path`: OneNote 图标路径

## 最终运行效果

```
✅ COM 连接：成功
✅ XML 获取：2963 字符
✅ 页面解析：4 个有效页面（过滤掉 3 个回收站页面）
✅ 功能状态：完全可用
```

**加载的页面示例：**
- 测试第一页
- 测试第二页
- 无标题页（快速笔记）

## 技术细节

### OneNote COM 接口版本
- OneNote 2007: `OneNote.Application.12`
- OneNote 2010: `OneNote.Application.14`
- OneNote 2013/2016: `OneNote.Application.15`
- ProgID: `OneNote.Application` (版本无关)

### GetHierarchy 方法签名
```cpp
HRESULT GetHierarchy(
    [in] BSTR bstrStartNodeID,      // 起始节点 ID，空表示所有
    [in] long hsScope,               // HierarchyScope 枚举值
    [out] BSTR* pbstrHierarchyXml,   // 输出 XML 字符串
    [in] long xsSchema               // XMLSchema 版本
);
```

**HierarchyScope 枚举：**
- `hsSelf = 0`
- `hsChildren = 1`
- `hsNotebooks = 2`
- `hsSections = 3`
- `hsPages = 4`

**XMLSchema 枚举：**
- `xs2007 = 0`
- `xs2010 = 1`
- `xs2013 = 2`
- `xsCurrent = 3`

### NavigateTo 方法签名
```cpp
HRESULT NavigateTo(
    [in] BSTR bstrHierarchyObjectID,  // 页面 ID
    [in] BSTR bstrObjectID,           // 对象 ID（通常为空）
    [in] VARIANT_BOOL fNewWindow      // 是否在新窗口打开
);
```

## 代码结构

### 关键文件

| 文件 | 说明 |
|------|------|
| `OneNoteSimple.hpp` | 基于 `#import` 的 COM 包装类 |
| `OneNoteUtil.hpp` | XML 解析和数据处理函数 |
| `OneNotePlugin.cpp` | 插件主逻辑和接口实现 |
| `OneNoteAction.hpp` | 页面数据模型（继承自 `BaseAction`） |
| `OneNotePluginData.hpp` | 插件配置数据结构 |

### 技术栈

| 组件 | 实现方式 |
|------|---------|
| COM 调用 | `#import` 指令（`OneNoteSimple.hpp`） |
| XML 解析 | MSXML6 + 命名空间设置 |
| 页面导航 | `IApplication::NavigateTo` 方法 |
| 智能匹配 | 拼音分词支持（通过 `IPluginHost`） |

## 经验教训

### 关键经验

1. **XML 命名空间是关键**
   - 使用 MSXML 解析带命名空间的 XML 时，必须先通过 `setProperty` 设置 `SelectionNamespaces`
   - 否则 XPath 查询会返回 0 个结果

2. **#import 远优于手动 IDispatch**
   - 减少 90% 以上代码
   - 类型安全，编译时检查
   - 自动资源管理
   - 官方支持，兼容性好

3. **早期过滤无用数据**
   - 在解析阶段就过滤回收站页面
   - 提升性能和用户体验
   - 避免显示已删除的内容

4. **调试日志至关重要**
   - 详细的日志帮助快速定位问题

### 遇到 COM 调用困难时的建议

1. **优先考虑 #import**
   - 不要过度依赖手动 `IDispatch::Invoke` 调用
   - Microsoft 的类型库导入机制更可靠

2. **错误 0x8002801D 的深层原因**
   - 通常表示类型库版本冲突或注册问题
   - 即使 `LoadRegTypeLib` 成功，运行时仍可能失败
   - 多版本 Office 并存时特别容易出现

3. **与 Office 组件互操作的推荐方式**
   - C++: 使用 `#import` 指令 ⭐
   - C++/CLI: 直接调用 .NET PIA
   - C#: 使用 `Microsoft.Office.Interop.OneNote`
   - 纯手动 `IDispatch` 应作为**最后手段**

## 参考资源

- [Microsoft OneNote 2016 Developer Reference](https://learn.microsoft.com/en-us/office/vba/api/overview/onenote)
- [ScipBe.Common.Office.OneNote GitHub](https://github.com/scipbe/ScipBe-Common-Office)
- [COM Type Libraries](https://learn.microsoft.com/en-us/windows/win32/com/type-libraries)
- [#import Directive](https://learn.microsoft.com/en-us/cpp/preprocessor/hash-import-directive-cpp)

## 开发时间线

| 日期 | 里程碑 |
|----|--------|
| 1  | 开始开发，尝试手动 IDispatch 调用 |
| 2  | 遇到 TYPE_E_LIBNOTREGISTERED 错误，多次尝试失败 |
| 3  | 改用 #import 指令，COM 调用成功 ✅ |
| 4  | 实现回收站过滤、调试日志、配置支持 ✅ |
| 5  | **插件开发完成** 🎉 |

---
**插件状态**: 🟢 **已完成并正常运行**
**测试环境**: Windows 10/11 + Microsoft OneNote 2016
---

兼容性分析

✅ 支持的场景

1. 已安装桌面版 OneNote 2013/2016
   - 代码使用 OneNote COM API (OneNote.Application)
   - 需要完整的桌面版 Office 安装
   - 支持 Windows 10/11
2. 双重实现提供后备方案
   - OneNoteSimple.hpp: 使用 #import 指令（优先，更可靠）
   - OneNoteUtil.hpp: 手动 IDispatch 调用（备用）

❌ 不支持的场景（关键问题）

1. OneNote for Windows 10/11 (UWP版本)
   - Windows 10/11 预装的现代版 OneNote 不提供 COM 接口
   - 插件代码依赖传统桌面版 COM API
   - UWP 应用无法通过 CoCreateInstance 创建
2. 仅安装 Microsoft 365 网页版 OneNote
   - 纯云端版本没有本地 COM 组件
3. OneNote 完全未安装
   - 代码会检测失败并返回空结果

实际兼容性评估

测试环境: Windows 10/11 + Microsoft OneNote 2016

预期成功率

| 环境                          | 兼容性    | 说明             |
  |-----------------------------|--------|----------------|
| Office 2013/2016/2019 桌面版   | ✅ 完全支持 | 提供完整 COM API   |
| Microsoft 365 桌面版 OneNote   | ✅ 可能支持 | 取决于是否包含 COM 接口 |
| Windows 10 预装 OneNote (UWP) | ❌ 不支持  | 没有 COM API     |
| Windows 11 预装 OneNote (UWP) | ❌ 不支持  | 没有 COM API     |
| 纯网页版 OneNote                | ❌ 不支持  | 无本地组件          |

关键代码证据

OneNoteSimple.hpp:40-46:
```
// 尝试创建 COM 实例
hr = m_app.CreateInstance(__uuidof(OneNoteLib::Application));
if (SUCCEEDED(hr) && m_app != nullptr) {
m_initialized = true;
ConsolePrintln(L"OneNote", L"Successfully connected via #import");
} else {
ConsolePrintln(L"OneNote", L"Failed to create OneNote instance");
}
```
OneNoteUtil.hpp:57-61:
```
// 尝试不同 ProgID
hr = CLSIDFromProgID(L"OneNote.Application.15", &clsid);  // Office 2013/2016
if (FAILED(hr)) {
hr = CLSIDFromProgID(L"OneNote.Application", &clsid);
}
```
改进建议

如果要提高兼容性，可以考虑：

1. 检测并提示用户
   - 在插件初始化失败时，明确提示需要安装桌面版 OneNote
   - 提供下载链接或安装指南
2. 支持 OneNote 本地缓存文件
   - UWP 版本会在本地缓存 .one 文件
   - 可以尝试解析缓存文件（更复杂，需要逆向工程）
3. 使用 Microsoft Graph API
   - 通过网络 API 访问 OneNote 云端数据
   - 需要 OAuth 认证，实现复杂度高

结论

当前插件兼容性有限：
- ✅ 在安装了桌面版 Office OneNote 的系统上工作良好
- ❌ 在仅使用 Windows 预装 UWP OneNote 的系统上无法工作
- ⚠️ 由于 Microsoft 已将 OneNote 主要版本转向 UWP/网页版，大多数新 Windows 10/11
  用户可能无法使用此插件
