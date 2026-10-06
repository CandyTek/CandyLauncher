# CandyLauncher 皮肤

此目录存放启动器皮肤。程序会递归查找文件名以 `skin_` 开头的 JSON 文件，并将其列在「设置 → 选择皮肤」中。选择自定义皮肤时，请将「夜间模式」设为「总是使用主题」；强制夜间模式或跟随系统深色模式可能覆盖当前皮肤。

## 现有皮肤

| 文件 | 说明 |
| --- | --- |
| `skin_default.json` | 默认皮肤，也是新皮肤的字段参考。 |
| `skin_night.json` | 内置夜间皮肤。 |
| `skin_anime_starlight.json` | 浅粉色透明浮窗星夜樱花皮肤，背景位于 `anime_starlight/background.png`。 |
| `skin_cowgirl_samurai.json` | 浮窗牛仔武士皮肤，透明背景让人物伸出搜索面板。 |
| `skin_moonlit_shrine.json` | 月夜神社少女，紫色搜索面板。 |
| `skin_ocean_captain.json` | 海洋船长少女，深蓝与青绿色搜索面板。 |
| `skin_forest_alchemist.json` | 森林炼金少女，深绿搜索面板。 |
| `skin_celestial_astronomer.json` | 星象学者少女，深蓝与金色搜索面板。 |
| `skin_retro_arcade.json` | 复古街机少女，洋红与青色搜索面板。 |
| `skin_winter_courier.json` | 冬日信使少女，冰蓝搜索面板。 |
| `skin_desert_mechanic.json` | 沙漠机械师少女，琥珀色搜索面板。 |
| `skin_cherry_tea_artist.json` | 樱花茶艺少女，酒红与玫瑰金搜索面板。 |

`skin_test_bk.json` 和 `skin_test_bk2.json` 是测试备份文件。

## 新建皮肤

1. 复制 `skin_default.json`，命名为 `skin_名称.json`，保留需要的布局字段，再修改尺寸、位置、字体和颜色。
2. 将图片放在 `skins` 下的独立文件夹。图片路径以单个反斜杠开头时，相对于当前 JSON 文件所在目录；JSON 中需要将反斜杠写成 `\\`，例如 `"window_bg_picture": "\\anime_starlight\\background.png"`。留空字符串表示不使用图片。
3. 使用 UTF-8 编码保存文件。构建程序时，CMake 会把 `skins` 目录复制到可执行文件旁边。

星夜樱花皮肤使用的网上素材、商业使用许可及透明背景制作记录见 [`anime_starlight/README.md`](anime_starlight/README.md)。其 `window_shape_from_bg_alpha` 选项会按背景 PNG 的 alpha 通道裁切窗口。
浮窗牛仔武士皮肤的 CC0 原图、背景图及制作记录见 [`cowgirl_samurai/README.md`](cowgirl_samurai/README.md)。其 `window_shape_from_bg_alpha` 选项会按背景 PNG 的 alpha 通道裁切窗口；只有需要这种窗口形状的皮肤才应启用。

新增的八款少女皮肤各有独立素材目录和生成记录，背景图由内置 imagegen 工具原创生成。它们也启用 `window_shape_from_bg_alpha`，使面板和人物以外的透明区域不阻挡桌面点击。
