# 新增主题背景生成记录

以下十六张 `background.png` 使用内置 imagegen 工具原创生成，调用时设置 `transparent_background: true`。每张图片为 1536 × 1024 RGBA PNG，放在同名主题目录中。所有皮肤以左侧约 40% 区域放置实际搜索框和结果列表。

## 提示词

除 Outrun 与 Steampunk 外，其他主题均使用以下英文公共提示词，再接表中的主题描述：

> Use case: stylized-concept. Asset type: CandyLauncher desktop skin background. Wide 3:2 landscape composition. Reserve left half as a spacious simple rectangular surface for real search controls and result text. Detailed art concentrated on right side and borders. Background art only; no UI controls, no text, no letters, no logos, no watermark. Transparent canvas outside the illustrated card silhouette. Theme:

| 主题 | 接续的主题描述 |
| --- | --- |
| Green-on-black terminal | Vintage phosphor CRT screen, subtle scanlines, green monochrome circuit traces and code-like abstract glyph shapes only on right edge; flat nearly-black left panel. #07120B and luminous #54E68A. No legible text. |
| 8-bit pixel art | Pixelated fantasy landscape with block clouds, tiny stars and colorful tile motifs on right, quiet dark navy left panel. Sharp square pixel edges, no smoothing. #16233C, #F5D56B, #69C9D1. |
| Frutiger Aero | Glossy translucent bubbles, water ripples, fresh green hill, bright blue sky and soft lens light on right, quiet pale aqua glass panel on left. Early 2000s optimistic desktop illustration. #DDF8F5, #6AC3D7, #368E88. |
| Art Nouveau | Flowing floral vines, iris blooms and graceful gold organic curves around right and border, quiet warm cream panel on left. Elegant hand-painted decorative poster style. #F5EBD4, #4A665C, muted gold. |
| Bauhaus | Overlapping circles, triangles and rectangles in red, cobalt, mustard and ivory on right and outer edge; quiet warm ivory panel on left. Flat screenprinted geometry, disciplined asymmetric grid. |
| Memphis | Playful zigzags, confetti dots, squiggles and bold geometric shapes on right border; quiet cream panel on left. Teal, coral, lilac, yellow and black accents, 1980s graphic design. |
| Pop Art/comic | Halftone dots, bright speech-bubble shapes without words, bold black contour lines, dynamic starburst rays on right; quiet ivory panel on left. Cherry red, sunshine yellow, cobalt and cyan. |
| Impressionist/watercolor | Loose brushstrokes and translucent watercolor washes depicting flowers, pond and soft trees on right, quiet pale paper panel on left. Lilac, sage green, pale blue and warm peach. |
| Low-poly | Faceted polygon mountains, fir trees, river and geometric sky on right; quiet slate-blue polygon panel on left. Angular clean 3D paper-craft appearance, teal, blue and mint. |
| Skeuomorphic | Richly textured dark walnut wood frame, stitched brown leather inset, cream paper sheets and small brass hardware as right-side decoration; quiet smooth cream paper panel on left for legible text. Tactile realistic materials. |
| Spring | Cherry blossoms, fresh leaves, light rain droplets and clear sky on right; quiet pale blush panel on left. Soft botanical illustration, pink, mint and warm cream. |
| Summer | Sunny beach, turquoise sea, bright citrus slices and tropical leaves on right; quiet pale sand panel on left. Joyful painterly illustration, aqua, coral and golden yellow. |
| Christmas | Evergreen branches, warm string lights, ornaments, wrapped gifts and gentle snow on right; quiet deep pine green panel on left. Cozy festive illustration, forest green, crimson and gold. |
| Lunar New Year | Red lanterns, plum blossoms, gold cloud motifs and subtle fireworks on right; quiet deep red panel on left. Elegant Chinese festival illustration, vermilion, jade and warm gold; no zodiac animal, no calligraphy. |

Outrun 使用独立提示词：1980 年代 synthwave 夜间公路、低垂日落、洋红和青色霓虹、棕榈树与跑车；左侧深紫色干净面板，右侧海岸公路；外缘透明，无文字和界面控件。

Steampunk 使用独立提示词：黄铜齿轮、铜管和维多利亚工业机械集中在右侧；左侧深色皮革面板，暖琥珀光；外缘透明，无文字和界面控件。

所有背景均由生成工具生成，JSON 中的字体颜色、输入框底色与选中行颜色按可读性单独设置。

## 新增景观与角色主题（十一款）

以下背景同样由内置 imagegen 工具逐张原创生成，调用时设置 `transparent_background: true`，使用 1536 × 1024 的横向 PNG。各主题使用下面的公共构图要求，并接续表中的主题描述：

> Use case: stylized-concept. Asset type: CandyLauncher desktop skin background. Wide 3:2 landscape card with genuinely transparent outer canvas. Reserve the left 45 percent as a calm, simple panel for real search controls and result text. Concentrate detailed art on the right and outer border. Background art only; no UI controls, text, letters, logos, or watermark.

| 主题 | 接续的主题描述 |
| --- | --- |
| Underwater coral | Underwater coral reef, luminous turquoise water, branching coral, anemones, tiny tropical fish, shafts of sunlight and drifting bubbles. Sophisticated painterly illustration; deep navy and aqua with warm coral accents. Left panel deep teal. |
| Aurora polar | Polar aurora sweeping over snowy mountains and a frozen lake, delicate stars, teal and violet light. Crisp Nordic storybook illustration. Left panel dark midnight blue. |
| Rainforest | Lush rainforest canopy, broad tropical leaves, vines, waterfall, mist, jewel-toned birds and dappled sun. Rich hand-painted adventure landscape; emerald, jade, lime and gold. Left panel dark botanical green. |
| Sakura garden | Serene sakura garden landscape, flowering cherry trees, curved path, pond bridge, distant hills and drifting petals. Soft watercolor and ink painting; blush pink, sage green and pale blue. No people. Left panel pale warm cream. |
| Volcano | Dramatic volcano above dark basalt cliffs, distant flowing lava, ember sparks and smoky sunset sky. Cinematic painterly fantasy landscape; obsidian, copper and ember orange, friendly rather than frightening. Left panel near-black charcoal. |
| Desert landscape | Sweeping desert dunes, sandstone mesas, oasis palms and distant caravan trail at golden hour. Atmospheric painterly landscape; ochre, terracotta and indigo shadows. No people. Left panel warm sand. |
| Stargazer boy | Friendly androgynous young adult stargazer in a practical cloak with short hair, holding a brass telescope beneath a starry sky and crescent moon. Original anime-inspired character illustration; violet, navy and gold. Left panel deep indigo. |
| Chibi cloud keeper | Cute original chibi cloud keeper mascot with oversized rain boots and small umbrella, sitting among fluffy clouds, tiny stars and rainbow light. Rounded storybook illustration; lilac, mint, peach and cream. Left panel pale lavender. |
| Cat and fox companions | Adorable fluffy orange cat and little red fox resting together on a garden bench under autumn leaves. Playful faces, cozy picture-book illustration; cinnamon, amber, teal and cream. No humans. Left panel warm cream. |
| Clockwork mecha | Friendly compact retro-futuristic robot mecha with rounded armored panels and glowing cyan visor, standing in a workshop with small gears and soft lights. Original clean sci-fi concept illustration; cobalt, silver, cyan and amber. Left panel dark steel blue. |
| Western cartoon adventurer | Cheerful original Western cartoon style androgynous adventurer with short curly hair, simple jacket and backpack, on a sunlit cliff overlooking rolling hills. Bold expressive outlines, flat animation cel style; coral, sky blue, mustard and sage. Clearly non-anime. Left panel warm cream. |

以上主题的字体颜色、输入框底色与选中行颜色分别在同名 `skin_*.json` 中设置。背景图片置于同名文件夹的 `background.png`；窗口继续使用 PNG alpha 通道裁切。
