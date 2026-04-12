# 主窗口绘制流程

## 1. 主窗口显示入口

主窗口显示入口在 `util/MainTools.hpp` 的 `ShowMainWindowSimple()`。

当前流程：

1. `RestoreWindowIfMinimized(g_mainHwnd)`
2. `SetFocus(g_editHwnd)`
3. `MyMoveWindow(g_mainHwnd)`
4. 如果存在 `g_BgImage`，调用 `RedrawWindow(...)`
5. `SetForegroundWindow(g_mainHwnd)`
6. 如有输入法句柄，发送 `WM_INPUTLANGCHANGEREQUEST`

其中 `MyMoveWindow(g_mainHwnd)` 会进入不同的窗口定位分支，最终通过 `SetWindowPos(... SWP_SHOWWINDOW ...)` 把主窗口显示出来。

## 2. 主窗口重绘消息链

主窗口消息过程在 `CandyLauncher.cpp`。

相关链路如下：

1. 收到 `WM_PAINT`
2. 调用 `BeginPaint(hWnd, &ps)`
3. 调用 `MainWindowCustomPaint(ps, hdc)`
4. 如果 `MainWindowCustomPaint()` 没有提前返回，则执行 `EndPaint(hWnd, &ps)`

对应代码位置：

- `CandyLauncher.cpp:239` 开始处理 `WM_PAINT`
- `common/AppController.hpp:473` 定义 `MainWindowCustomPaint()`

## 3. 为什么系统不会先帮我们清背景

主窗口在 `CandyLauncher.cpp` 里显式处理了 `WM_ERASEBKGND`：

```cpp
case WM_ERASEBKGND:
{
    return 1;
}
```

这表示窗口告诉系统“背景我自己处理，你不要擦除”。

这样做本身没问题，但前提是 `WM_PAINT` 里的自绘逻辑必须每次都从一块干净的底图开始。如果自绘直接在旧像素上继续混合，半透明内容就会不断累加。

## 4. 这次问题出现的原因

问题现象：

- 主界面带背景图片
- 背景图片包含透明或半透明区域，例如阴影
- 主窗口频繁显示、隐藏、激活、重绘后，阴影区域越来越黑

旧逻辑的关键点：

1. `WM_ERASEBKGND` 不擦背景
2. `MainWindowCustomPaint()` 在有 `g_BgImage` 时直接 `graphics.DrawImage(g_BgImage, rect)`
3. 对于半透明像素，`DrawImage()` 会和当前目标表面的旧内容继续混合

所以每次重绘都不是“重新画一张完整背景”，而是“把带透明度的图再次叠到之前已经画过的结果上”。阴影这类黑色半透明区域最容易不断积累，最后看起来就会发黑。

## 5. 这次修复做了什么

修复位置在 `common/AppController.hpp:477` 附近。

现在有背景图时，绘制流程改成：

1. 根据 `ps.rcPaint` 计算本次需要重绘的区域 `rect`
2. 创建一张新的 `Gdiplus::Bitmap backBuffer(rect.Width, rect.Height, PixelFormat32bppPARGB)`
3. 用 `bufferGraphics.Clear(Gdiplus::Color(0, 0, 0, 0))` 把离屏缓冲区清成透明
4. 把 `g_BgImage` 画到这张全新的离屏缓冲区上
5. 用 `CompositingModeSourceCopy` 把整块离屏结果直接复制回窗口

核心变化不是“多重绘一次”，而是“每次都先在一张全新的透明画布上生成结果，再整体覆盖回主窗口”。

## 6. 为什么这次改动生效了

这次能生效，是因为它解决的是根因，不是症状。

旧逻辑的问题根因：

- 目标窗口表面上已经有旧像素
- 半透明背景图继续和旧像素做混合

新逻辑的关键效果：

- 每次绘制都先创建新的离屏位图
- 离屏位图在绘制前被显式清空为透明
- 背景图先画到这张干净位图上
- 最后用 `SourceCopy` 直接覆盖目标区域，而不是继续叠加混合

所以这次每一帧看到的阴影，都只来自当前这一轮背景图本身，不再包含上一次、上上次留下的阴影累积结果。

## 7. `ShowMainWindowSimple()` 里的补充刷新

`ShowMainWindowSimple()` 里额外加的：

```cpp
if (g_BgImage != nullptr) {
    RedrawWindow(g_mainHwnd, nullptr, nullptr,
        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
}
```

这段代码的作用是：

- 当主界面使用背景图时，显示窗口后主动请求一次整窗重绘
- 尽快把新的绘制逻辑跑起来

它有帮助，但它不是这次修复真正生效的核心原因。真正解决“积黑”的，是 `MainWindowCustomPaint()` 里改成了“离屏重建 + SourceCopy 覆盖”。

## 8. 后续维护注意点

以后如果主窗口背景还要继续支持：

- 半透明 PNG
- 磨砂/玻璃效果
- 分区域局部刷新

建议保持下面两个原则不变：

1. 不要直接把半透明背景图反复画到旧窗口内容上
2. 每次绘制背景时，都先在干净缓冲区里生成完整结果，再整体提交到窗口

如果以后又把背景绘制改回直接 `DrawImage(hdc)`，`WM_ERASEBKGND` 又继续返回 `1`，这个“越用越黑”的问题大概率还会回来。
