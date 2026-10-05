# ScreenToolsPlugin

搜索 **截长图** 或 **框选识别二维码** 并执行。两个功能都用鼠标拖动选择屏幕区域，按 Esc 取消。

- 截长图：框选可滚动的内容区域。插件在选区中心发送滚轮事件，逐帧截取并寻找重叠部分，最多拼接 30 帧或 30000 像素高，保存为“图片/CandyLongShot_*.png”。请把鼠标停在可滚动内容内，且不要在截图期间操作页面。固定导航栏、动画、视频或滚轮不响应时，拼接可能提前结束；此时仍会保存已取得的画面。
- 框选识别二维码：使用 zbar 识别所选区域中的 QR Code，显示内容并复制到剪贴板；有多个二维码时用换行分隔。

首次 CMake 配置需要网络获取固定提交的 [zbar Windows 版](https://github.com/iredt/zbar-windows)（LGPL-2.1-or-later）和 [win-iconv](https://github.com/win-iconv/win-iconv)（公有领域）。PNG 保存使用仓库内的 [stb_image_write](https://github.com/nothings/stb)；许可见 `STB_LICENSE.txt`。
