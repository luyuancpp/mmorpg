# Q Daoist UI Controls V2

这一版重新绘制为高清控件切图，不再从整张 UI 截取。可交互控件都拆成两层：

1. `backgrounds/*_bg.png`：底色、形状、可拉伸主体。
2. `foregrounds/*_fg.png`：描边、高光、角花、云纹等前景装饰。

推荐渲染顺序：背景层 -> 客户端文字/动态图标 -> 前景层。
服务器状态不要再用红圆点或灰圆点，改用 `icons/badge_*`、`icons/ornament_jade_chip.png` 或客户端文字状态。

`composites/` 只用于预览，不建议直接作为最终控件使用。
`preview/controls_overview.png` 是总览图。

`icons/` 里补了从 `q_daoist_login_clear_2560x1080.png` 效果图拆出来的小 icon：
服务器状态点、服务器圆章、左侧分类符号、底栏选点、顶部 tab 玉扣/回纹等。`atlas/qdao_login_icon_alts_atlas.png`
和同名 json 是这些小 icon 的 atlas 预览/坐标，实际接入时也可以继续按独立 png 使用。
