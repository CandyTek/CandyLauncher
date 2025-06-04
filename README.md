## Repository overview

This project builds a small Windows launcher. CMakeLists.txt configures the build for C++17 and links various Win32/GDI+ libraries:

```
1  cmake_minimum_required(VERSION 3.31)
2  project(WindowsProject1)
5  set(CMAKE_CXX_STANDARD 17)  # ...
...
55  target_link_libraries(WindowsProject1
56          gdiplus
57          comctl32
58          kernel32
59          user32
...
68          shlwapi
69  )
```

The main entry point is in WindowsProject1.cpp, which sets up the window, initializes GDI+, and enters the message loop:

```
40  // 主进程函数
41  int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
42                                        _In_opt_ HINSTANCE hPrevInstance,
43                                        _In_ LPWSTR lpCmdLine,
44                                        _In_ int nCmdShow)
...
62          while (GetMessage(&msg, nullptr, 0, 0))
63          {
64                  if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
65                  {
66                          TranslateMessage(&msg);
67                          DispatchMessage(&msg);
68                  }
69          }
```

Important constants (UI colors, executable folder) are defined in DataKeeper.hpp:

```
6  // const int COLOR_UI_BG_VALUE = 110;
8  const int COLOR_UI_BG_VALUE = 170;
9  const int COLOR_UI_BG_DEEP_VALUE = 130;
11 const COLORREF COLOR_UI_BG = RGB(COLOR_UI_BG_VALUE, COLOR_UI_BG_VALUE, COLOR_UI_BG_VALUE);
12 const COLORREF COLOR_UI_BG_DEEP = RGB(COLOR_UI_BG_DEEP_VALUE, COLOR_UI_BG_DEEP_VALUE, COLOR_UI_BG_DEEP_VALUE);
17 const std::wstring EXE_FOLDER_PATH=GetExecutableFolder();
19 // 定义回调函数类型，它接受一个整数参数，返回一个整数
20 typedef int (*CallbackFunction)(int);
```

A RunCommandAction encapsulates how a command gets executed. The constructor builds a search key (using pinyin) and sets title/subtitle:

```
14 class RunCommandAction : public ActionBase
...
17      RunCommandAction(
18              const std::wstring& justName,
19              const std::wstring& targetFilePath,
20              const bool isUwpItem = false,
...
24              : IsUwpItem(isUwpItem),
25              RunCommand(PinyinHelper::GetPinyinLongStr(MyToLower(justName))),
26              targetFilePath(targetFilePath),
27              defaultAsAdmin(admin), // 用拼音转换工具时替换
28              workingDirectory(workingDir.empty() ? GetDirectory(targetFilePath) : workingDir),
29              arguments(std::move(args))
```

ListViewManager creates and draws the search results list. It loads available actions and filters them by user input:

```
75  void ListViewManager::Initialize(HWND parent, HINSTANCE hInstance, const int x, const int y, const int width,
... 
83          const DWORD style = LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_CHILD | WS_VISIBLE | LVS_OWNERDRAWFIXED |
84                  LVS_NOCOLUMNHEADER;
...
102  // 加载快捷方式项到列表里
103  void ListViewManager::LoadActions(const std::vector<std::shared_ptr<RunCommandAction>>& actions)
...
129  // 编辑框筛选列表内容
130  void ListViewManager::Filter(const std::wstring& keyword)
...
145          for (const auto& action : allActions)
...
159                  if (allMatch)
160                  {
...
171                          ListView_SetItemText(hListView, item.iItem, 1, const_cast<LPWSTR>(subtitle.c_str()));
...
174                          matchedCount++;
175                          if (matchedCount >= 7)
176                                  break;
```

ListedRunnerPlugin reads runner.json from the executable folder and generates RunCommandAction objects. It also handles folder traversal with filters:

```
30  ListedRunnerPlugin::ListedRunnerPlugin(const CallbackFunction callbackFunction1)
31  {
32          callbackFunction=callbackFunction1;
33          configPath = EXE_FOLDER_PATH + L"\\runner.json";
34          LoadConfiguration();
35  }
...
69  void ListedRunnerPlugin::LoadConfiguration()
70  {
71          actions.clear();
72          std::shared_ptr<RefreshAction> refreshAction = std::make_shared<RefreshAction>(callbackFunction);
73          actions.push_back(refreshAction);
74          std::ifstream in((configPath.data())); // 用 std::ifstream 而不是 std::wifstream
75          if (!in)
76          {
77                  std::wcerr << L"配置文件不存在：" << configPath << std::endl;
78                  return;
79          }
...
116                          if (cmd.contains("exePath"))
...
136                                  std::wstring folderPath = Utf8ToWString(cmd["folder"].get<std::string>());
137                                  if (!FolderExists(folderPath))
138                                  {
139                                          std::wcerr << L"指定的文件夹不存在：" << folderPath << std::endl;
140                                          continue;
                                  }
...
187                                  TraverseFiles(folderPath, traverseOptions, actions);
```

<br>
<br>

---

### Key components

- WindowsProject1.cpp – sets up the main window, registers a global hotkey, manages the message loop, and handles events.

- RunCommandAction.cpp – represents a runnable command or file item.

- ListedRunnerPlugin – loads commands from runner.json and can scan directories for executables/shortcuts.

- ListViewManager – displays items and implements filtering and owner-drawn display.

- EditHelper – subclass procedure for the search box; processes keystrokes (e.g., Enter or arrow keys).

- TrayMenuManager – manages the system tray icon and context menu.

### Getting started tips

- Build with CMake or the provided Visual Studio solution (WindowsProject1.sln).

- Start reading from WindowsProject1.cpp to see how the app launches, then explore ListViewManager and ListedRunnerPlugin to understand how commands are loaded and displayed.

- runner.json in the executable folder defines custom commands and directories to scan; reviewing its expected structure (see LoadConfiguration logic) is important.

- Many helper functions for UI behavior reside in MainTools.hpp (window positioning, global hotkey registration, context menu helpers, etc.).

- For handling Pinyin and search, PinyinHelper.h offers conversions and large mapping tables.
