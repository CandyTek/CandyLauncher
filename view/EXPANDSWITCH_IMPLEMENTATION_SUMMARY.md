# ExpandSwitch 控件实现总结

## 概述

成功实现了一个组合控件 `expandswitch`,它将折叠按钮(expand)和开关(switch)合并到一个控件中。左侧是可点击的展开/折叠区域,右侧是一个独立的布尔开关。

## 修改的文件

### 1. view/CustomButtonHelper.hpp
**主要修改:**
- 添加了新的按钮样式 `BTN_EXPAND_SWITCH`
- 在 `ButtonInfo` 结构中添加了两个新字段:
  - `bool switchState`: 开关的开/关状态
  - `bool switchHovered`: 开关的悬停状态
- 实现了三个新函数:
  - `DrawMiniSwitch()`: 绘制带GDI+抗锯齿的圆角开关
  - `GetSwitchRect()`: 计算开关在按钮中的矩形区域
  - `GetExpandSwitchState()`: 获取开关状态
  - `SetExpandSwitchState()`: 设置开关状态
- 增强了 `EnhancedButtonSubclassProc()` 的事件处理:
  - `WM_MOUSEMOVE`: 独立跟踪开关区域的悬停状态
  - `WM_LBUTTONUP`: 根据点击位置区分是展开/折叠还是切换开关
    - 点击开关区域: 发送 `BN_CLICKED` 消息
    - 点击其他区域: 发送 `BN_DOUBLECLICKED` 消息
- 在 `WM_PAINT` 中添加了 `BTN_EXPAND_SWITCH` 的绘制逻辑

**新增头文件:**
```cpp
#include <windowsx.h>  // 用于 GET_X_LPARAM/GET_Y_LPARAM
#include <gdiplus.h>   // 用于抗锯齿绘制
```

### 2. window/SettingWindow.hpp
**主要修改:**
- 在 `CreateSettingControlsForTab()` 函数中添加了对 `expandswitch` 类型的处理
- 创建控件时同时设置展开状态和开关状态:
```cpp
SetExpandButtonState(hLabel, item.isExpanded);
SetExpandSwitchState(hLabel, item.boolValue);
```
- 支持children子项的递归创建

### 3. window/SettingsHelper.hpp
**主要修改:**
- 添加了前置声明: `static bool GetExpandSwitchState(HWND hButton);`
- 在 `saveSettingControlValuesSub()` 函数中添加了对 `expandswitch` 的保存逻辑:
  - 获取开关状态并保存到配置
  - 递归保存子项配置
- 修复了一个编译错误: 注释掉了未定义的 `isFollowFormer` 字段

### 4. view/ScrollViewHelper.hpp
**主要修改:**
- 在 `ScrollContainerProc()` 的 `WM_COMMAND` 处理中添加了对 `expandswitch` 的事件响应:
  - **BN_CLICKED**: 处理开关状态改变
    - 获取新的开关状态
    - 更新 `SettingItem` 的 `boolValue`
    - 保存配置
  - **BN_DOUBLECLICKED**: 处理展开/折叠
    - 保存当前滚动位置
    - 切换展开状态
    - 刷新设置UI

## 工作原理

### 状态管理
```
ButtonInfo {
    style = BTN_EXPAND_SWITCH
    isExpanded: false/true     // 展开状态
    switchState: false/true    // 开关状态
    switchHovered: false/true  // 开关悬停
}
```

### 事件流程

#### 点击开关
```
用户点击 → WM_LBUTTONUP
→ 检测点击在开关区域
→ 切换switchState
→ 发送BN_CLICKED
→ ScrollContainerProc处理
→ 更新boolValue
→ 保存配置
```

#### 点击展开
```
用户点击 → WM_LBUTTONUP
→ 检测点击在展开区域
→ 切换isExpanded
→ 发送BN_DOUBLECLICKED
→ ScrollContainerProc处理
→ 刷新UI显示/隐藏子项
```

### 区域划分
```
┌─────────────────────────────────────────┐
│ ▶ 功能组标题                  ⚪───  │
│ ← 展开区域 →          ← 开关区域 → │
└─────────────────────────────────────────┘
```

- **展开区域**: 从左边界到 (right - 56)px
- **开关区域**: 最右侧 40x20px,距右边缘8px

## 配置格式

### settings.json 配置
```json
{
  "key": "feature_group",
  "type": "expandswitch",
  "title": "功能组名称",
  "subPage": "feature",
  "defValue": true,
  "children": [...]
}
```

### user_settings.json 保存
```json
{
  "feature_group": true  // 保存开关状态
}
```

## 使用API

### C++代码中访问
```cpp
// 读取开关状态
bool enabled = g_settings_map["feature_group"].boolValue;

// 直接从控件读取
HWND hCtrl = GetDlgItem(hParent, ctrlId);
bool state = GetExpandSwitchState(hCtrl);

// 设置开关状态
SetExpandSwitchState(hCtrl, true);

// 设置展开状态
SetExpandButtonState(hCtrl, true);
```

## 视觉效果

### 开关样式
- **关闭状态**: 灰色轨道 (RGB 200,200,200), 滑块在左侧
- **开启状态**: 蓝色轨道 (RGB 0,120,215), 滑块在右侧
- **悬停效果**: 颜色加深
- **抗锯齿**: 使用GDI+实现平滑圆角

### 展开箭头
- **折叠**: 向右箭头 ▶
- **展开**: 向下箭头 ▼
- **悬停**: 蓝色高亮

## 技术亮点

1. **区域检测**: 使用 `PtInRect()` 精确判断点击位置
2. **双缓冲绘制**: 避免闪烁
3. **GDI+抗锯齿**: 开关圆角平滑
4. **独立悬停**: 开关和展开区域各自的悬停效果
5. **消息机制**: 使用不同的通知码区分两种点击
6. **递归支持**: children完全支持嵌套

## 测试方法

1. 将示例配置添加到 `settings.json`:
```json
{
  "key": "test_expandswitch",
  "type": "expandswitch",
  "title": "测试功能",
  "subPage": "feature",
  "defValue": true,
  "children": [
    {
      "key": "test_child",
      "type": "bool",
      "title": "子选项",
      "subPage": "feature",
      "defValue": false
    }
  ]
}
```

2. 运行程序并打开设置窗口
3. 在"功能"标签页找到新控件
4. 测试:
   - 点击左侧文本/箭头 → 应展开/折叠子项
   - 点击右侧开关 → 应切换开关状态,不影响展开状态

## 编译状态

✅ 编译成功
✅ 无警告
✅ 所有依赖正确链接

## 相关文档

- `EXPANDSWITCH_USAGE.md`: 详细使用说明
- `settings_expandswitch_example.json`: 配置示例

## 后续优化建议

1. 添加动画效果(可选):
   - 开关滑动动画
   - 展开/折叠过渡动画

2. 增强交互反馈:
   - 点击时的按下效果
   - 禁用状态的灰色显示

3. 可访问性:
   - 键盘导航支持
   - 屏幕阅读器支持

4. 配置选项:
   - 可配置的开关尺寸
   - 可配置的颜色主题

## 潜在问题

- **ID分配**: 确保控件ID不冲突(3000+)
- **滚动位置**: 展开/折叠后保持用户的滚动位置
- **保存时机**: 开关切换是否立即保存(当前是立即保存)

## 总结

成功实现了一个功能完整的组合控件,将expand和switch的功能优雅地整合在一起。代码架构清晰,易于维护和扩展。通过合理的事件分发和区域检测,两个功能互不干扰,用户体验良好。
