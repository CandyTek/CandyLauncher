当最后一个设置项操作没响应的时候，就说明子subid 算错了，3xxx最后一个控件id和 g_settingsmap 的数量没有对应（3075 对应 g_settingsmap.size() 75），界面的控件数量与实际数组没有对齐
[ScrollViewController.hpp](../view/ScrollViewController.hpp) 断点 case WM_COMMAND: if (ctrlId >= 3000 && ctrlId < static_cast<int>(3000 + g_settings_map.size())) {

