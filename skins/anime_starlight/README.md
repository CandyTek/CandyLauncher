# 星夜樱花皮肤素材

- `hachiimages2-cc0.png`：HachiStudio 的角色图，取自 [OpenGameArt 的 Character Imageset 页面](https://opengameart.org/content/character-imageset)，页面标注 **CC0**。原始下载地址：<https://opengameart.org/sites/default/files/hachiimages2.png>。
- `background.png`：以该角色图和上一版星夜樱花背景为参考，使用内置 `image_gen` 工具制作的透明 PNG。左侧为浅粉色搜索面板，右侧完整呈现从头到鞋的站姿角色；面板和角色轮廓之外是真实的 alpha 透明区域。保留银色及黄粉渐变发色、橙色服饰，以及少量樱花、星光和装饰线。无文字或标志。

原图与主题背景均随皮肤文件一同提供。OpenGameArt 页面标注原图为 CC0，可用于商业项目；保留原图和来源说明便于后续核查。

皮肤文件 `../skin_anime_starlight.json` 启用 `window_shape_from_bg_alpha`，程序按 PNG 的 alpha 通道裁切窗口；透明区域不会挡住桌面点击。`window_shape_alpha_threshold` 设置轮廓裁切阈值。搜索框和结果列表位于左侧不透明面板内。

## 背景制作提示词

使用内置 `image_gen` 工具，输入图片为上方的 CC0 角色图和上一版星夜樱花背景。提示词：

> Use case: compositing. Asset type: complete CandyLauncher skin background PNG, landscape approximately 3:2, with genuine transparency outside the panel and character. Input images: the previous anime_starlight background for the floating-window layout and sakura/star ornaments; the HachiStudio CC0 character sheet for the full-body character, including both shoes. Repaint the same single adult anime woman FULL LENGTH, head to both feet inside the canvas. Keep silver hair blending into yellow and pink tips, golden eyes, orange dress, white sleeves, dark bow, black stockings and orange shoes. Place her on the RIGHT roughly 33–37% of the canvas with natural proportions. Draw a large solid opaque LIGHT PINK rounded search panel on the LEFT, approximately x=3%–67%, y=16%–94%; keep its interior smooth and clear for dark plum search and result text. Add restrained sakura blossoms, star glints and thin rose-gold borders near panel edges. The woman may overlap only the panel's right edge. Make every area outside the panel and her silhouette truly transparent alpha=0. No scenery outside the panel, UI controls, text, logos, checkerboard, extra people, duplicate limbs, cropped head or cropped feet.
