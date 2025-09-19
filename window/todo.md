# 设置框架重构计划

## 当前问题分析

### 1. 架构问题
- **单一职责违反**: `SettingWindow.hpp` 混合了UI创建、事件处理、数据管理
- **全局状态过多**: 大量inline全局变量导致状态管理复杂
- **紧耦合**: UI逻辑与数据逻辑深度耦合，难以测试和维护

### 2. 具体技术问题
- **控件ID映射复杂**: 通过计算偏移量(3000+i)来映射控件与数据
- **内存管理混乱**: 手动管理大量HWND句柄，容易泄漏
- **滚动实现笨重**: 通过手动移动控件位置实现滚动，性能差
- **折叠功能低效**: 需要销毁重建整个UI才能更新折叠状态

## 重构目标

### 1. 架构目标
- 采用MVC/MVP模式实现关注点分离
- 使用RAII和智能指针管理资源
- 实现类型安全的数据绑定
- 支持插件式扩展新的设置类型

### 2. 性能目标
- 减少不必要的控件重建
- 实现高效的滚动机制
- 优化折叠/展开动画效果
- 降低内存占用

## 新架构设计

### 1. 核心组件

```cpp
// 设置项接口
class ISettingItem {
public:
    virtual ~ISettingItem() = default;
    virtual void CreateControl(HWND parent, int& y, int width) = 0;
    virtual void LoadValue(const nlohmann::json& value) = 0;
    virtual nlohmann::json SaveValue() = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual int GetHeight() const = 0;
    virtual std::string GetKey() const = 0;
};

// 设置页面管理器
class SettingPage {
private:
    std::vector<std::unique_ptr<ISettingItem>> items_;
    HWND container_hwnd_;
    std::wstring name_;
    std::wstring display_name_;

public:
    void AddItem(std::unique_ptr<ISettingItem> item);
    void CreateControls(HWND parent);
    void LoadData(const nlohmann::json& data);
    nlohmann::json SaveData();
    void SetVisible(bool visible);
    void RefreshLayout();
};

// 主设置对话框
class SettingDialog {
private:
    std::vector<std::unique_ptr<SettingPage>> pages_;
    nlohmann::json original_data_;
    nlohmann::json current_data_;
    HWND hwnd_;
    int current_page_index_ = 0;

public:
    void AddPage(std::unique_ptr<SettingPage> page);
    void Show(HWND parent);
    void SwitchToPage(int index);
    bool SaveSettings();
    void CancelSettings();
};
```

### 2. 控件类型系统

```cpp
// 布尔值设置
class BoolSettingItem : public ISettingItem {
private:
    std::string key_;
    std::wstring title_;
    std::unique_ptr<SwitchControl> control_;
    bool default_value_;

public:
    BoolSettingItem(const std::string& key, const std::wstring& title, bool default_val);
    // 实现ISettingItem接口...
};

// 字符串设置
class StringSettingItem : public ISettingItem {
private:
    std::string key_;
    std::wstring title_;
    HWND edit_hwnd_ = nullptr;
    std::string default_value_;
    bool multiline_ = false;

public:
    StringSettingItem(const std::string& key, const std::wstring& title,
                     const std::string& default_val, bool multiline = false);
    // 实现ISettingItem接口...
};

// 列表选择设置
class ListSettingItem : public ISettingItem {
private:
    std::string key_;
    std::wstring title_;
    HWND combo_hwnd_ = nullptr;
    std::vector<std::string> entries_;
    std::vector<std::string> values_;
    std::string default_value_;

public:
    ListSettingItem(const std::string& key, const std::wstring& title,
                   const std::vector<std::string>& entries,
                   const std::vector<std::string>& values,
                   const std::string& default_val);
    // 实现ISettingItem接口...
};

// 可折叠组设置
class ExpandableGroupItem : public ISettingItem {
private:
    std::string key_;
    std::wstring title_;
    std::vector<std::unique_ptr<ISettingItem>> children_;
    bool expanded_ = false;
    HWND expand_button_hwnd_ = nullptr;

public:
    ExpandableGroupItem(const std::string& key, const std::wstring& title);
    void AddChild(std::unique_ptr<ISettingItem> child);
    void SetExpanded(bool expanded);
    // 实现ISettingItem接口...
};
```

### 3. 工厂模式支持

```cpp
class SettingItemFactory {
public:
    static std::unique_ptr<ISettingItem> CreateFromJson(const nlohmann::json& config);

private:
    static std::unique_ptr<ISettingItem> CreateBoolItem(const nlohmann::json& config);
    static std::unique_ptr<ISettingItem> CreateStringItem(const nlohmann::json& config);
    static std::unique_ptr<ISettingItem> CreateListItem(const nlohmann::json& config);
    static std::unique_ptr<ISettingItem> CreateExpandableGroup(const nlohmann::json& config);
    // 其他类型...
};
```

## 重构实施计划

### 阶段1: 基础架构 (1-2天)
1. **创建核心接口和基类**
    - [ ] 实现 `ISettingItem` 接口
    - [ ] 创建 `SettingPage` 类
    - [ ] 创建 `SettingDialog` 类
    - [ ] 实现基础的资源管理机制

2. **实现基础控件类型**
    - [ ] `BoolSettingItem` (开关控件)
    - [ ] `StringSettingItem` (文本输入)
    - [ ] `ListSettingItem` (下拉选择)
    - [ ] `TextSettingItem` (静态文本)

### 阶段2: 高级功能 (2-3天)
3. **实现高级控件类型**
    - [ ] `ExpandableGroupItem` (可折叠组)
    - [ ] `HotkeySettingItem` (热键输入)
    - [ ] `ButtonSettingItem` (按钮操作)
    - [ ] `NumberSettingItem` (数值输入)

4. **实现工厂模式**
    - [ ] `SettingItemFactory` 类
    - [ ] JSON配置解析器
    - [ ] 类型注册机制

### 阶段3: UI优化 (2天)
5. **优化滚动实现**
    - [ ] 使用虚拟滚动技术
    - [ ] 实现平滑滚动动画
    - [ ] 优化大量控件的性能

6. **改进布局系统**
    - [ ] 自动布局管理器
    - [ ] 响应式布局支持
    - [ ] 控件间距和对齐优化

### 阶段4: 数据层重构 (1-2天)
7. **重构数据管理**
    - [ ] 移除全局变量依赖
    - [ ] 实现配置数据模型
    - [ ] 添加数据验证机制
    - [ ] 实现撤销/重做功能

8. **优化性能**
    - [ ] 延迟加载机制
    - [ ] 控件重用池
    - [ ] 内存使用优化

### 阶段5: 迁移和测试 (1-2天)
9. **渐进式迁移**
    - [ ] 创建兼容性适配器
    - [ ] 逐页面迁移现有设置
    - [ ] 保持向后兼容性

10. **测试和优化**
- [ ] 功能测试
- [ ] 性能测试
- [ ] 内存泄漏检查
- [ ] 用户体验优化

## 技术要点

### 1. 内存管理
```cpp
// 使用智能指针自动管理资源
class SettingPage {
    std::vector<std::unique_ptr<ISettingItem>> items_;  // 自动清理
    WindowHandle container_{nullptr, DestroyWindow};    // RAII窗口句柄
};
```

### 2. 事件处理
```cpp
// 使用观察者模式处理事件
class ISettingItemObserver {
public:
    virtual void OnValueChanged(const std::string& key, const nlohmann::json& value) = 0;
};

class SettingItem {
    std::vector<ISettingItemObserver*> observers_;
protected:
    void NotifyValueChanged() {
        for (auto* observer : observers_) {
            observer->OnValueChanged(key_, GetCurrentValue());
        }
    }
};
```

### 3. 类型安全
```cpp
// 使用模板实现类型安全的设置访问
template<typename T>
class TypedSettingItem : public ISettingItem {
    T value_;
    T default_value_;
public:
    void SetValue(const T& value) { value_ = value; }
    T GetValue() const { return value_; }
    void Reset() { value_ = default_value_; }
};
```

## 预期收益

### 1. 可维护性提升
- 代码量减少约30-40%
- 新增设置类型只需实现接口
- 单元测试覆盖率可达90%+

### 2. 性能改进
- 内存使用减少约20%
- 设置页面切换速度提升50%
- 折叠/展开操作延迟降低至100ms以内

### 3. 扩展性增强
- 支持第三方插件添加设置页面
- 支持主题和皮肤系统集成
- 支持配置导入/导出功能

## 风险评估

### 1. 技术风险
- **中等风险**: 需要大量重构现有代码
- **缓解措施**: 采用渐进式迁移策略

### 2. 兼容性风险
- **低风险**: 保持现有JSON配置格式兼容
- **缓解措施**: 创建适配器层

### 3. 时间风险
- **中等风险**: 预计需要1-2周完成
- **缓解措施**: 分阶段实施，优先核心功能

## 结论

当前的设置框架确实存在严重的设计问题，建议采用上述重构方案。通过引入现代C++设计模式和最佳实践，可以显著提升代码质量、性能和可维护性。

重构应该分阶段进行，优先实现核心功能，然后逐步迁移现有代码，确保系统稳定性的同时完成架构升级。
