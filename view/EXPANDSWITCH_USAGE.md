# ExpandSwitch 控件使用说明

## 概述

ExpandSwitch 是一个组合控件,左侧是可折叠的expand按钮(带箭头),右侧是一个开关(switch)。这个控件适用于需要在折叠功能的同时提供开关功能的场景。

## 功能特点

- **左侧展开/折叠按钮**: 点击左侧区域(箭头和文本)可以展开或折叠子项
- **右侧开关**: 点击右侧的开关可以切换布尔状态
- **独立交互**: 展开/折叠和开关状态互不影响
- **支持子项**: 像普通expand一样支持children子项
- **悬停效果**: 开关区域有独立的悬停高亮效果

## 在settings.json中的配置示例

```json
{
  "key": "my_feature_group",
  "type": "expandswitch",
  "title": "我的功能组",
  "subPage": "feature",
  "defValue": true,
  "children": [
    {
      "key": "child_option_1",
      "type": "bool",
      "title": "子选项1",
      "subPage": "feature",
      "defValue": false
    },
    {
      "key": "child_option_2",
      "type": "string",
      "title": "子选项2",
      "subPage": "feature",
      "defValue": "默认值"
    }
  ]
}
```

## 配置字段说明

- `key`: 配置项的唯一标识符
- `type`: 必须设置为 `"expandswitch"`
- `title`: 显示的文本标题
- `subPage`: 所属的设置页面标签
- `defValue`: 开关的默认状态 (true/false)
- `children`: 子配置项数组,当展开时显示

## 代码中的使用

### 获取开关状态

```cpp
// 通过key从g_settings_map获取值
bool isEnabled = g_settings_map["my_feature_group"].boolValue;

// 或者直接从控件获取
HWND hControl = GetDlgItem(hParent, controlId);
bool state = GetExpandSwitchState(hControl);
```

### 设置开关状态

```cpp
HWND hControl = GetDlgItem(hParent, controlId);
SetExpandSwitchState(hControl, true); // 设置为开启
```

### 设置展开状态

```cpp
HWND hControl = GetDlgItem(hParent, controlId);
SetExpandButtonState(hControl, true); // 设置为展开
```

## 事件处理

控件会发送两种类型的WM_COMMAND消息:

### 1. 开关状态改变

- 通知码: `BN_CLICKED`
- 当用户点击右侧开关时触发
- 在 `ScrollViewHelper.hpp` 的 `ScrollContainerProc` 中处理

### 2. 展开/折叠状态改变

- 通知码: `BN_DOUBLECLICKED`
- 当用户点击左侧区域时触发
- 会刷新整个设置界面以显示/隐藏子项

## 视觉设计

- **左侧箭头**: 展开时向下(▼), 折叠时向右(▶)
- **开关尺寸**: 40x20像素
- **开关位置**: 距离右边缘8px
- **文本区域**: 左侧留28px给箭头, 右侧留56px给开关
- **背景色**: 继承expand按钮的浅灰色(RGB 245,245,245)
- **悬停效果**: 开关悬停时颜色加深

## 实现文件

- `view/CustomButtonHelper.hpp`: 控件绘制和交互逻辑
- `window/SettingWindow.hpp`: 控件创建和初始化
- `window/SettingsHelper.hpp`: 配置加载和保存
- `view/ScrollViewHelper.hpp`: 事件处理
- `common/GlobalState.hpp`: SettingItem结构定义

## 技术细节

### 控件结构

- 使用 `ButtonInfo` 结构存储状态
    - `isExpanded`: 展开状态
    - `switchState`: 开关状态
    - `switchHovered`: 开关悬停状态

### 绘制技术

- 使用GDI+绘制圆角开关
- 双缓冲避免闪烁
- 抗锯齿渲染

### 区域检测

- `GetSwitchRect()`: 计算开关的矩形区域
- 根据鼠标点击位置判断是点击开关还是展开区域

## 注意事项

1. **保存逻辑**: expandswitch类型会保存开关的布尔值到配置文件
2. **子项递归**: children子项会递归处理,支持嵌套
3. **ID分配**: 每个控件消耗一个唯一ID(从3000开始)
4. **配置合并**: 用户配置会自动合并到默认配置

## 示例场景

### 场景1: 功能组开关

```json
{
  "key": "advanced_features",
  "type": "expandswitch",
  "title": "高级功能",
  "subPage": "feature",
  "defValue": true,
  "children": [
    {
      "key": "feature_a",
      "type": "bool",
      "title": "功能A",
      "subPage": "feature",
      "defValue": false
    },
    {
      "key": "feature_b",
      "type": "bool",
      "title": "功能B",
      "subPage": "feature",
      "defValue": true
    }
  ]
}
```

当"高级功能"开关打开时,整个功能组启用;当展开时,可以看到具体的子功能选项。

### 场景2: 插件配置

```json
{
  "key": "plugin_enabled",
  "type": "expandswitch",
  "title": "插件名称",
  "subPage": "plugin",
  "defValue": false,
  "children": [
    {
      "key": "plugin_option1",
      "type": "string",
      "title": "选项1",
      "subPage": "plugin",
      "defValue": "默认值"
    },
    {
      "key": "plugin_option2",
      "type": "long",
      "title": "选项2",
      "subPage": "plugin",
      "defValue": 100
    }
  ]
}
```

用开关控制插件启用/禁用,展开可以配置插件的具体参数。
