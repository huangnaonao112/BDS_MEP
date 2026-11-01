# 架构说明

## 1. 分层

```
ui ──────────────► services ──────────► core
 │                    │                   ▲
 │                    └── widgets         │
 └──────────────────► widgets ────────────┘
```

| 层 | 目录 | 依赖 | 职责 |
|---|---|---|---|
| core/gnss | `src/core/gnss` | QtCore | 坐标、时间、历元模型、文件解析 |
| core/evaluation | `src/core/evaluation` | core/gnss | 对齐、统计、报告、明细输出 |
| core/project | `src/core/project` | QtCore | 项目配置读写 |
| core/track | `src/core/track` | core/gnss | ENU 轨迹容器、UI 解析门面 |
| services | `src/services` | core | AppController、会话状态、后台任务 |
| ui | `src/ui` | services + widgets | 窗口、对话框（被动视图） |
| widgets | `src/widgets` | core | 可复用控件（含日志面板） |
| widgets/plot | `src/widgets/plot` | QtGui | 相机、索引、LOD、渲染帧、渲染器、轨迹控件 |

硬规则：

- core/services 不 include 任何 `QWidget`/ui 头；
- 新增解析格式只改 `core/gnss`；
- 新增统计指标只改 `EvaluationResult` + `ReportBuilder` + 测试。

## 2. 一次评估的调用链

```
MainWindow::onEvaluate                      // 只转发，不校验
  └─ AppController::evaluate                // 校验 + 组装请求 + 置 busy 状态
       ├─ emit stateChanged(busy=true)      // 视图画进度条
       └─ EvaluationService::start(request) // 拷贝 request 到工作线程
            └─ ev::runEvaluation(request, sink)
            ├─ gnss::autoParseFile(baseline)    // 格式嗅探 + 解析
            ├─ gnss::autoParseFile(evaluation)
            ├─ ComparisonWriter::open(outDir)
            ├─ ev::evaluate(base, eval, opts, sink)
            │    ├─ thinSeries(1Hz)
            │    ├─ normalizeSeries()           // 展开跨零点 + 排序 + 补 BLH/ECEF
            │    ├─ attachDate()                // 无日期一侧补日期
            │    ├─ ecefToEnu() × N
            │    ├─ 双指针匹配
            │    └─ computeStats() / ThresholdTable::compute()
            ├─ writer.finalize(result)
            ├─ ReportBuilder::buildText/buildHtml
            └─ 写 evaluation_report.txt/html
  ◄─ progress/log/finished 信号回到 UI 线程（经 AppController 转发/落状态）
```

## 3. 线程模型

| 任务 | 执行位置 | 取消 |
|---|---|---|
| 文件解析（导入、点轨迹） | `QtConcurrent` 线程池 | 不支持（解析通常 <1 s） |
| 数据评估 | `QtConcurrent` 线程池 | 支持，阶段边界检查 |
| 报告渲染（HTML） | 工作线程 | 随评估取消 |
| UI 更新 | 主线程 | — |

跨线程只传值类型：`ImportOutcome`、`EvaluationOutcome`（含 `EvaluationResult`、QString），不共享可变状态。旧的 `DataEvaluator` 成员变量存结果、多线程直接读写的写法被删除。

`EvaluationService` 析构时 `cancel()` + `waitForFinished()`，防止关闭窗口时工作线程访问已销毁对象。

## 4. 项目持久化

`project.config` 为 INI：

```ini
[Project]
Name=2025-07-02-test
Path=C:/data/projects/2025-07-02-test
Created=2026-10-18 19:39:10
Version=2

[Files]
Baseline/Path=base.pos
Baseline/Format=POS
Baseline/Epochs=2879
Baseline/Start=2025-07-02 00:00:00.000
Baseline/End=2025-07-02 23:59:30.000
Baseline/Interval=30.0
Baseline/FixRate=99.41
Baseline/DisplayName=base.pos (POS基准)

[Results]
files=Fix_Fix.txt, Fix_Float.txt, Fix_Single.txt, evaluation_report.txt, evaluation_report.html
```

- 路径一律存**相对项目目录**，整个目录搬走仍可打开；
- 打开项目时校验文件是否存在，缺失的记录自动忽略；
- 旧版（只有 `Name/Path`）的配置可直接打开，只是没有文件记录；
- 任务树/表格从配置恢复，无需重新解析；点击轨迹时才按需解析。

## 5. 绘图管线

分层（都在 `src/widgets/plot/` 下，控件 `rtkplotwidget` 只做输入与缓存）：

```
Camera2D            世界↔屏幕唯一变换源；可比较、可插值
TrackSeries         不可变点集 + bounds + revision（缓存失效只认 revision）
TrackIndex          均匀网格；查询返回"索引区间"，每个区间两侧各扩一个点
TrackLod            抽稀到 maxPoints，强制保留每个区间的首尾（边界锚点）
TrackRenderModel    查询 + 抽稀 + 坐标变换 → RenderFrame（屏幕坐标几何）
PlotRenderer        setClipRect + 画折线/标记/误差圆/箭头；不接触原始数据
RTKPlotWidget       输入 → Camera2D；QPixmap 缓存 key = (camera, revision)
```

关键契约：

- **可见性只在 `TrackRenderModel` 决定**，渲染层拿到的是屏幕坐标几何，不会再"顺手过滤"；
- 任何与视口相交的线段都必须出现在 `RenderFrame` 里（端点可以落在视口外）；
- `TrackIndex` 按"相邻点最大间距 L"扩张查询范围（相交线段的某个端点必然落在
  距视口不超过 L 的位置），再对候选点的前后邻居做线段-矩形求交，把横穿画布的
  线段补进来；扩张区候选点过多时（>20 万）跳过该兜底以保证流畅；
- **裁剪而非剔除**：`PlotRenderer` 设 `clipRect`，轨迹线和标记在边缘呈"半截"而不是提前消失。
  这是绘图库的通行做法（matplotlib 的 `clip_box`、GIS 的 clip region 同理）。

缓存的失效条件收敛成一个键：`(Camera2D, TrackSeries::revision, viewport size, style)`。
不再有 `m_flush / cacheValid / needsFit / autoFit` 这类需要手动维护的标志位
（`autoFit` 仅表示"窗口变化时是否自动重新适配"）。

`RTKPlotWidget` 交互：

- 拖拽直接调 `Camera2D::panByPixels`，**不再手工 16 ms 节流**（`update()` 自带合并，
  手工门限会丢掉少量移动事件，表现为一顿一顿）；
- 滚轮以鼠标为锚点缩放，`Camera2D::zoomAt` 保证锚点下的世界坐标不动；
  不丢弃高频滚轮事件，连续滚动时在动画目标上累加，避免"每次从当前值重新动画"；
- 缩放动画是**时间驱动**的（180 ms ease-out + `Qt::PreciseTimer`），掉帧只会少几帧、
  不会变慢；动画期间 `renderLevel=0`（4000 点 LOD、隔行刻度标签、无标记点预算上限 1000）；
- 交互期间轨迹线仍开抗锯齿（成本 <0.5 ms），避免折线"棱角化"被误认为卡顿；
- 松手后立即重建全质量帧（约 7 ms），仍在 16.7 ms 预算内。

量化结果见 `docs/benchmark.csv`。

## 6. 扩展点

| 想做什么 | 改哪里 |
|---|---|
| 支持新的观测格式 | 新增 `core/gnss/xxxparser.cpp`，在 `parsercommon.cpp` 注册 |
| 新增统计指标 | `EvaluationResult` 加字段 → `ReportBuilder` 两个模板 → 测试 |
| 报告加图表 | `ReportBuilder::buildHtml` 生成 `<img>`（内嵌 base64 PNG），数据来自 `EvaluationResult` |
| 换存储格式（JSON/SQLite） | `ProjectManager` 内部实现，外部接口不变 |
| 支持多文件批量评估 | 复用 `EvaluationService`，把 `--output` 改为每个项目一个子目录 |
