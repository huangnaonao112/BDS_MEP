# ADR 0001：轨迹绘图管线分层（Camera2D / TrackIndex / TrackRenderModel / PlotRenderer）

- 状态：已采用
- 背景：画布边缘的点和连线会提前消失；修复时发现根因不是某个函数写错，而是
  "可见性由数据层决定、渲染层只拿到子集"这个结构问题。

## 问题

旧实现 `RTKPlotData::getPointsInViewport()` 同时做空间索引、视口过滤、粗筛/精筛、
抽稀、排序，返回"应该画的点"。渲染函数拿这个子集直接 `drawPolyline`：

- 与视口相交、但端点都在视口外的线段永远拿不到；
- 边缘点被剔除后，标记也一起消失；
- "裁剪"无从谈起，只能在渲染层补相邻点打补丁；
- 契约是隐式的（必须排序、必须带边界点、必须断点分段），没有测试能守住。

## 决定

按绘图库通行的分层拆分（`src/widgets/plot/`）：

| 层 | 职责 | 不做什么 |
|---|---|---|
| `Camera2D` | 世界↔屏幕唯一变换；可比较、可插值 | 不知道数据 |
| `TrackSeries` | 不可变点集 + bounds + revision | 不知道视口 |
| `TrackIndex` | 网格索引；返回**索引区间**，每段两侧各扩一点 | 不做抽稀、不画 |
| `TrackLod` | 抽稀到预算，强制保留每段首尾 | 不做可见性判断 |
| `TrackRenderModel` | 可见性 + 抽稀 + 坐标变换 → `RenderFrame`（屏幕坐标） | 不接触 QPainter |
| `PlotRenderer` | `setClipRect` + 画折线/标记/误差圆/箭头 | 不接触原始数据 |
| `RTKPlotWidget` | 输入 → 相机、缓存调度 | 不碰几何过滤 |

关键契约（写进 `TrackRenderModel` 头文件并由属性测试守住）：

> 任何与 `camera.visibleWorld()` 相交的线段，都必须出现在 `RenderFrame` 的折线里；
> 端点允许落在视口外，由渲染层的 `clipRect` 裁齐。

实现要点：

1. `TrackIndex` 返回区间而不是点，区间两侧各扩一个数据点（边界锚点）；
2. 视口内没有点时，按"相邻点最大间距 L"扩张查询范围，对候选点的前后邻居做
   线段-矩形求交，把横穿画布的线段补回来；
3. `PlotRenderer` 显式设置裁剪矩形，边缘呈半截线/半个标记；
4. 缓存 key 收敛为 `(Camera2D, TrackSeries::revision, viewport, style)`，
   删除 `m_flush / cacheValid / needsFit` 等手工标志。

## 备选方案

- **QGraphicsScene/QGraphicsView**：自带裁剪与空间索引，但百万点的 item 开销、
  自定义误差圆/速度箭头的适配成本都高，且 Qt 场景图对"一帧内 1 万顶点"没有优势。
- **全量绘制 + 裁剪**：几何上最正确，但 100 万点每帧 16 ms 预算撑不住；
  仍需要 LOD，只是把 LOD 留在渲染循环里，更难测试。
- **线段索引（每段写入经过的网格）**：可以做到完全精确，但构建成本约 O(n) 次
  跨格写入，百万点加载会明显变慢；当前"最大线段长度扩张 + 邻居求交"在典型
  1 Hz 轨迹上与精确解等价，极端超长线段才可能漏。

## 后果

- 正面：可见性只有一个决策点；渲染层可独立替换；几何契约可脱离 QPainter 测试；
  边缘裁切、横穿线段、空视口都有回归用例。
- 负面：放大 100× 时百万点建帧要 2.55 ms（线段求交兜底），仍是 16 ms 预算的 1/6；
  绘制约 5000 个标记点占单帧 6.8 ms，是下一步优化的首选目标。
- 限制：扩张区候选点超过 20 万（或网格单元 >2 万）时会跳过线段求交兜底，
  退回只画视口内的区间；常规缩放不会触发，真触发时应改用线段索引。

## 验证

- `tests/ui`：`trackIsClippedAtCanvasEdge`（边缘像素）、
  `renderFrameCoversVisibleSegments`（随机轨迹 × 20 视口 + 横穿空视口的长线段，
  独立用 Liang-Barsky 校验契约）；
- 基准：`docs/benchmark.csv`、`docs/benchmark-1m.csv`。
