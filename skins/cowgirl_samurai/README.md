# 浮窗牛仔武士皮肤素材

- `source-cc0.png`：VinessaGlair 发布的 [Cowgirl Samurai](https://opengameart.org/content/cowgirl-samurai) 角色设定图。页面标注 **CC0**，并说明美术由 monoartstudio 受委托制作。原始下载地址：<https://opengameart.org/sites/default/files/png_transparent_file_0.png>。
- `background.png`：使用内置 `image_gen` 工具，参考原图中的正面人物制作的透明 PNG。左侧是深蓝搜索面板，右侧人物的帽子和身体伸出面板；面板和人物之外的像素保留 alpha 透明。

皮肤文件 `../skin_cowgirl_samurai.json` 启用 `window_shape_from_bg_alpha`，程序据图片 alpha 通道裁切窗口。透明区域不会挡住桌面点击。`window_shape_alpha_threshold` 决定边缘裁切阈值。

## 背景制作提示词

> Use case: compositing. Asset type: a complete CandyLauncher skin background PNG with real transparent pixels, landscape roughly 3:2. Input image is a CC0 commissioned anime cowgirl samurai character sheet; use ONLY its FRONT VIEW adult woman as the character reference. Keep her recognizable brown western hat, short black hair, warm tan face, white kimono-style blouse with red ties, dark corset and brown leather accents. Change her T-pose into a natural relaxed standing pose, one hand resting at her hip. Compose an opaque elegant midnight-navy search panel on the LEFT 60% from about 20% down to the bottom, with a subtle thin cyan/gold border and a quiet flat dark interior for UI text. The full anime woman stands on the RIGHT 40%, head/hat starting near the top and torso/legs extending below the panel, visibly protruding outside the panel silhouette. Make all areas outside the left panel and outside the woman's silhouette GENUINELY TRANSPARENT (alpha=0), including the top left 20% and upper middle; do not fill with scenery or color. Artwork should be polished 2D anime illustration. No interface controls, no text, no logos, no second character, no grey checkerboard. The outer silhouette should look clean at small window size.
