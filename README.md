# GNSS 数据评估系统（BDS_MEP）

桌面端 GNSS 观测质量评估工具：把两套解算结果（基准 / 评估）按时间对齐，给出解状态分布、误差分布、精度指标与可用性，并生成可交付的 HTML/PDF 报告与交互式轨迹图。

Qt 5.14+（兼容 Qt 6）· C++17 · qmake · 19 个单元测试 + 8 个 UI 用例（含 1 个默认跳过的截图用例） · 有渲染/解析基准数据

---

## 1. 它解决什么问题

同一个测段往往有多套解算结果：参考站解、终端解、不同算法/参数下的解。判断"哪套更好、差多少"，靠肉眼看轨迹图不可靠。这个工具做三件事：

1. **对齐**：两个文件按历元时间配对（支持带日期与只有时刻的 NMEA，支持跨零点）；
2. **量化**：在统一的 ENU 坐标系里统计平面/高程误差，给出 RMS、CEP50、95%、标准差、分位数、误差分布、连续可用性、初始化时间；
3. **交付**：输出 HTML 报告（可打印/导出 PDF）、文本报告、逐历元比较明细（`Fix_Fix.txt` / `Fix_Float.txt` / `Fix_Single.txt`）。

---

## 2. 功能

| 模块 | 能力 |
|---|---|
| 项目管理 | 新建/打开项目；`project.config` 持久化基准文件、评估文件、结果清单与文件元信息，重新打开项目时完整恢复（Ctrl+O 或任务树右键“打开项目…”） |
| 数据导入 | NMEA GGA、RTKLIB POS/STAT、UTC 表格三种格式；按内容嗅探格式；UTF-8/GBK 编码自动探测；后台线程解析 |
| 时间对齐 | 统一绝对时间轴；只有时刻的文件按参照文件补日期；跨零点先在文件顺序上展开再排序 |
| 精度统计 | 解状态分布、误差分布（分母明确）、RMS/CEP50/95%/σ/P68/P95/P99、连续可用性、初始化时间 |
| 报告 | 文本 + HTML 双格式；HTML 支持预览、打印、导出 PDF；报告写入项目目录并登记到任务树 |
| 轨迹图 | 空间索引 + 视口裁剪 + LOD 抽稀；像素缓存；平滑缩放/拖拽；按解状态着色 |
| 工程化 | 后台评估（进度条 + 取消）、命令行评估工具、单元测试 + UI 冒烟测试、基准脚本、一键构建/打包脚本 |

---

## 3. 架构

```
┌──────────────────────────── src/ui ────────────────────────────┐
│ MainWindow        只做"用户操作 → 调用 → 更新界面"              │
│ NewProjectDialog                                               │
└───────────────▲───────────────────────────────▲────────────────┘
                │ signals/slots                 │ 直接调用
┌───────────────┴────────── src/services ───────┴────────────────┐
│ ProjectSession   项目状态 + 轨迹缓存 + 导入事务                 │
│ ImportService    后台解析（QtConcurrent + QFutureWatcher）      │
│ EvaluationService 后台评估 + 进度 + 取消                        │
└───────────────▲───────────────────────────────▲────────────────┘
                │                               │
┌───────────────┴──── src/core/evaluation ──────┴────────────────┐
│ EvaluationEngine   对齐 + 统计（纯函数，可测）                  │
│ ErrorStats         RMS/分位/标准差                              │
│ ReportBuilder      文本/HTML 报告（模板 + 循环）                │
│ ComparisonWriter   逐历元明细输出                               │
│ EvaluationPipeline 解析→统计→写文件 的编排                      │
└───────────────▲────────────────────────────────────────────────┘
                │
┌───────────────┴──── src/core/gnss ─────────────────────────────┐
│ gnssdefs   质量码/椭球常量（全工程唯一来源）                    │
│ coordinates ECEF/BLH/ENU（全工程唯一实现）                      │
│ timeutil   时间解析/格式化（全工程唯一实现）                    │
│ epochdata  历元模型/规范化/抽稀                                 │
│ *parser    GGA / POS / TABULAR（每种格式一份实现）              │
└────────────────────────────────────────────────────────────────┘
        ▲                                   ▲
┌───────┴──── src/core/track ────┐  ┌──────┴──── src/widgets ─────┐
│ TrajectoryData（ENU 轨迹容器） │  │ plot/: Camera2D/TrackIndex  │
│ FileParser（UI 侧解析门面）    │  │ TrackLod/RenderModel/Renderer│
└────────────────────────────────┘  │ RTKPlotWidget/ProjectTree   │
                                    │ FileTableView/ReportPreview │
                                    └─────────────────────────────┘
```

数据流（评估）：

```
选择文件 → 复制进项目 → ImportService(后台) 解析 → ProjectSession 记录元信息
        → 数据评估 → EvaluationService(后台)
              parse(base) → parse(eval) → thin(1Hz) → normalize(时间轴)
              → ENU → 双指针匹配 → 分类统计 → 写明细 → 生成 txt/html
        → MainWindow 展示报告 + 登记结果文件
```

**分层约束**：`core` 与 `services` 不包含任何 UI 头文件；`tests` 只链接 core，可以无显示环境运行。

---

## 4. 目录结构

```
BDS_MEP/
├─ BDSMEP.pro                 # 应用工程
├─ main.cpp
├─ src/
│  ├─ version.h               # 版本等全局定义（单文件）
│  ├─ core/
│  │  ├─ gnss/                # 坐标、时间、历元模型、解析器、编码/文本读取
│  │  ├─ evaluation/          # 统计引擎、结果模型、报告、明细输出、评估管线
│  │  ├─ project/             # 项目持久化
│  │  └─ track/               # ENU 轨迹容器、UI 侧解析门面
│  ├─ services/               # AppController、会话、导入/评估后台服务
│  ├─ ui/                     # 主窗口、新建项目对话框
│  └─ widgets/                # 可复用控件
│     ├─ plot/                # 相机、索引、LOD、渲染帧、渲染器、轨迹控件
│     └─ debugoutput/...      # 日志面板、任务树、表格、报表预览、忙碌指示
├─ tests/                     # QtTest 单元测试（19 项）+ UI 冒烟测试（8 项）
├─ bench/                     # 渲染/解析基准 + 控件交互基准
├─ tools/cli/                 # 命令行评估工具
├─ scripts/                   # build / test / bench / deploy
├─ docs/                      # 架构、统计口径、ADR、基准结果
└─ data/                      # 示例数据
```

---

## 5. 构建与运行

依赖：Qt 5.14+（含 Concurrent / PrintSupport / Test），MinGW 7.3+ 或 MSVC 2017+，C++17。

```powershell
# Release 构建（自动定位 C:\Qt\Qt5.14.1\...，也可 -QtBin 指定）
powershell -ExecutionPolicy Bypass -File scripts\build.ps1

# 运行（可选 --open 直接打开项目）
.\build-release\BDSMEP.exe
.\build-release\BDSMEP.exe --open C:\data\projects\2025-07-02-test

# 单元测试 + UI 冒烟测试（19 + 8 项）
powershell -ExecutionPolicy Bypass -File scripts\test.ps1

# 渲染/解析基准
powershell -ExecutionPolicy Bypass -File scripts\bench.ps1 -ParseFile data\1_1_2_2025-07-02_rtk_raw.pos

# 打包（windeployqt + MinGW 运行库 → dist/）
powershell -ExecutionPolicy Bypass -File scripts\deploy.ps1
```

命令行评估（无界面，适合批量/CI）：

```powershell
.\build-cli\bdsmep-cli.exe -b data\gga_20251208_164245_984.txt `
                           -e data\gga_20251209_130826_898_adjusted_new.txt `
                           -o out\
```

退出码：`0` 成功且有匹配历元，`2` 成功但无匹配，`1` 失败。

Qt Creator 直接打开 `BDSMEP.pro` 也可以；测试工程是 `tests/tests.pro`（core）与 `tests/ui/ui.pro`（UI 冒烟），基准工程是 `bench/bench_plot.pro`。

---

## 6. 测试

`tests/tst_bdsmep.cpp`（QtTest，19 项，只链接 core）：

| 类别 | 用例 |
|---|---|
| 坐标 | BLH↔ECEF 往返、ENU 轴对应与往返、赤道弧长量级 |
| 质量码 | GGA→RTKLIB 归一化、单点解不再被当成固定解 |
| 解析 | GGA 经纬度/半球/质量、无效定位过滤、POS 跨零点保留日期、无日期回退 |
| 编码 | GBK 文件识别为 GB18030 且能正常解析 |
| 时间模型 | 跨零点先在文件顺序展开再排序、GGA 与 POS 混合时间基准补日期 |
| 统计口径 | **误差分布分母 = 基准固定解样本**、σ 与 P68 分离、RMS/中位数/95% |
| 匹配 | 容差拒绝错配、跨零点匹配、初始化时间 |
| 轨迹 | 缺测时完整率下降（旧实现恒 100%） |
| 项目 | 配置往返、旧版配置兼容、不存在的结果文件被过滤 |

`tests/ui/tst_ui.cpp`（真实 MainWindow + 绘图管线，offscreen 平台，8 项）：

| 用例 | 验证内容 |
|---|---|
| 工具栏布局 | 5 个按钮 + 2 个分隔符、标签与原版一致、**全部保持可用**、图标 32px |
| 点击"新建" | 真的弹出新建项目对话框（回归"按钮点了没反应"） |
| 打开项目 | 任务树出现基准/评估节点、表格两行、窗口标题带项目名 |
| 打开非法目录 | 返回 false，不弹模态、不改标题 |
| 端到端评估 | 点工具栏"数据评估" → 等待后台评估 → 校验报告、`Fix_Fix.txt`、预览页、结果登记；最后点"报表结果"验证视图切换 |
| 边缘裁切 | 采样点与视口边界错开时，折线仍画到画布第 0/399 列 |
| 可见性契约 | 400 点随机轨迹 × 20 组随机视口 + 一条横穿空视口的长线段；独立用 Liang-Barsky 算出"应出现"的线段，断言端点都在 `RenderFrame` 里 |
| 视觉快照 | 设置 `BDSMEP_SNAPSHOT_DIR` 时输出主界面/报表页/轨迹图 PNG，默认跳过 |

```powershell
powershell -ExecutionPolicy Bypass -File scripts\test.ps1
# core: Totals: 19 passed, 0 failed
# ui  : Totals: 9 passed, 0 failed, 1 skipped   （8 个用例 + init/cleanup，截图用例默认跳过）
```

# 需要人工看一眼界面时：
$env:BDSMEP_SNAPSHOT_DIR="$PWD\tmp\snapshots"; .\build-uitest\tst_ui.exe visualSnapshot
```

> 交互约定：工具栏按钮**永远保持可用**，前置条件不满足时由槽函数弹提示。
> 之前用 `setEnabled` 按状态置灰，用户点不动只会以为程序坏了——这条现在有测试守着。

> UI 测试曾经抓到一个真实致命 bug：`ProjectTreeWidget::reset()` 与
> `QAbstractItemView::reset()` 虚槽同名，`clear() → modelReset → reset() → clear()`
> 无限递归，打开项目即崩溃。现在改名为 `rebuild()`，这条规则写进了头文件注释。

---

## 7. 基准数据

环境：AMD Ryzen 5 7500F、Windows、Release `-O2`、Qt 5.14.1 MinGW 7.3、`QT_QPA_PLATFORM=offscreen`。
原始数据见 [`docs/benchmark.csv`](docs/benchmark.csv)、[`docs/benchmark-1m.csv`](docs/benchmark-1m.csv)。

| 场景 | 20 万点 | 100 万点 |
|---|---|---|
| 建空间索引（`TrackSeries` + `TrackIndex`） | 8.45 ms | 41.33 ms |
| 全图建帧（`TrackRenderModel`，约 1 万顶点） | 0.38 ms | 0.38 ms |
| 放大 100× 建帧（含线段求交兜底） | 0.48 ms（2026 点） | 2.55 ms（5124 点） |
| 离屏渲染一帧 1200×800（含约 5000 个标记） | 6.77 ms | 6.73 ms |
| 解析 397 KB POS（2879 历元） | 19 ms | — |

结论：

- **建帧耗时基本与原始点数无关**（0.38 ms），因为 `TrackIndex` 只返回可见索引区间、`TrackLod` 把顶点控制在 1 万以内；
- 放大 100× 时百万点要 2.55 ms，多出来的部分是"线段-矩形求交"兜底（保证横穿视口的线不丢），仍远低于 16 ms 帧预算；
- 单帧渲染 ~6.8 ms，主要成本是约 5000 个带描边的标记点，而不是轨迹折线（去掉标记后折线+网格+坐标轴只要 0.9 ms）；
- 百万点与二十万点的单帧耗时基本相同（6.7 ms），说明 LOD 生效。

交互流畅度（`bench/widget`，1200×800 窗口，控件级真实渲染）：

| 指标 | 20 万点 | 100 万点 |
|---|---|---|
| 拖动时单帧（4000 点、轨迹抗锯齿） | 0.41 ms | 0.35 ms |
| 缩放动画出帧间隔 | 16.0 ms（62.5 fps） | 16.0 ms（62.5 fps） |
| 松手后首帧（恢复全质量） | 7.6 ms | 6.8 ms |
| 缓存命中的重绘 | 0.15 ms | 0.17 ms |

结论：**60 Hz 不是瓶颈**。拖动时每帧只要 0.4 ms，缩放动画稳定 16 ms 出帧；松手后那一帧全质量重建 7 ms，也在 16.7 ms 预算内。
如果实际手感仍然发涩，优先怀疑窗口尺寸/缩放比、远程桌面或显卡合成器，而不是帧率不够。

---

## 8. 关键设计说明

### 8.1 时间模型（旧版最大的正确性隐患）

- 时间统一存成一个 `double`：
  - 文件带日期（POS、表格）→ UNIX 纪元起的绝对秒；
  - 只有时刻（NMEA GGA）→ 当天秒。
- `EpochSeries.hasDate` 记录属于哪种口径。
- 对齐时：两侧都有日期 → 直接比绝对时间；一侧只有时刻 → **按另一侧首个历元的日期补全**；跨零点在文件顺序上先展开成连续秒，再排序。
- 因此不再需要旧版散落在各处的 `±86400`、`fmod(sec-18+86400)` 补丁，也不再出现"把日期丢掉"导致的跨天错配。

### 8.2 统计口径

详见 [`docs/statistics.md`](docs/statistics.md)；绘图管线与 MainWindow 的取舍分别见 [`docs/adr/0001-plot-pipeline.md`](docs/adr/0001-plot-pipeline.md)、[`docs/adr/0002-mainwindow-mvp.md`](docs/adr/0002-mainwindow-mvp.md)。三条关键约定：

1. **误差分布的分母是"基准固定解匹配历元数"**，不是全部匹配历元。旧实现用全部匹配点做分母，当基准里混有浮点/单点解时占比被系统性压低。
2. **σ 是总体标准差，P68/P95/P99 是分位点，两者分开列出**。旧实现把 68.27% 分位叫"1σ"，面试被追问会很难解释。
3. 平面误差 = ENU 水平残差模长；高程误差 = `|ΔUp|`。

### 8.3 一份实现

| 逻辑 | 旧版 | 现在 |
|---|---|---|
| GGA 解析 | `DataEvaluator` + `FileParser` 两份 | `gnss/ggaparser.cpp` |
| POS 解析 | 两份 | `gnss/posparser.cpp` |
| 质量归一化 | 两处 switch | `gnssdefs.h::normalizeNmeaQuality` |
| ECEF/BLH/ENU | `TrajectoryData` + `DataEvaluator` 两份 | `gnss/coordinates.cpp` |
| GPS 时间转换 | 两份 `gpst2time` | `gnss/timeutil.cpp` |
| 轨迹→绘图点 | `MainWindow` 内两份（其中一份还是逐行复制的） | `mainwindow.cpp::toPlotPoints` 一处 |

### 8.4 头号"上帝类"拆解

旧 `MainWindow` 749 行，业务直接写在槽里。现在：

- 状态与文件事务 → `ProjectSession`
- 解析/评估重活 → `ImportService` / `EvaluationService`
- 统计与报告 → `core/evaluation`
- 主窗口只剩操作编排、视图切换、状态栏（约 480 行，含菜单/样式/提示文案）

### 8.5 线程模型

- 解析与评估都在 `QtConcurrent` 线程池执行，UI 线程只接收 `progressChanged` / `finished` 信号；
- 评估可取消（管线在阶段边界检查取消标志）；
- `EvaluationService` 析构时 `waitForFinished()`，避免退出时线程还在访问已销毁对象；
- 注意：`DataEvaluator` 式"成员变量存结果"的多线程写法被废弃，结果通过值语义的 `EvaluationOutcome` 回传。

---

## 9. 与旧版的差异（对照评审意见）

| 评审问题 | 处理 | 证据 |
|---|---|---|
| `parseGgaFile` / `gpst2time` / 质量归一化重复实现 | 收敛到 `src/core/gnss/` 单一实现，旧 `DataEvaluator`/`FileParser` 删除 | `git grep` 只有一份定义 |
| `parsePosFile` 丢日期、±86400 补丁 | 绝对时间模型 + 跨零点展开 + 补日期规则 | 测试 `posParserKeepsDateAcrossMidnight`、`engineAlignsAcrossMidnight` |
| 误差分布分母错误 | 分母改为基准固定解匹配数，报告显式打印分母 | 测试 `distributionDenominatorIsBaselineFix` |
| `calculateSigma` 名不符实 | 拆成 `stdPlane/stdHeight`（总体标准差）与 `p68/p95/p99`（分位点），报告并列 | 测试 `percentileDiffersFromStandardDeviation` |
| `EvaluationResult` 60+ 平铺字段 | 拆为 `FileSummary` / `ErrorStats` / `ThresholdTable` / `EvaluationResult` | `evaluationresult.h` |
| 报告 56 处 `report +=`、111 处 `html +=` | `ReportBuilder` 用模板 + 循环生成，百分比/分母集中计算 | `reportbuilder.cpp` |
| `MainWindow` 上帝类 | 拆出 AppController + AppState，窗口改为被动视图（749→480 行） | `docs/adr/0002-mainwindow-mvp.md` |
| 评估同步跑、界面卡死 | 后台线程 + 进度 + 取消 | `EvaluationService` |
| 项目配置只存名字路径 | INI 持久化文件、元信息、结果清单；支持"打开项目"（Ctrl+O / 右键菜单） | 测试 `projectRoundTrip` |
| 零测试 | 19 个 core 用例 + 8 个 UI 用例（offscreen，含可见性契约测试） | `tests/` |
| 无渲染量化 | 基准工具 + 数据 | `docs/benchmark*.csv` |
| 无 README / 无 git | 本文档 + `.gitignore` | 仓库根目录 |
| 窗口标题"故障检测仪软件" | 改为 `GNSS 数据评估系统 v1.0.0` | `mainwindow.ui` / `version.h` |
| `PdfView` 名不符实 | 重命名 `ReportPreview`，补齐打印/导出 PDF | `src/widgets/reportpreview.*` |
| 图标资源路径错误（`:/icons/zoom_in.png`） | 移除不存在的图标引用，改用文字按钮 | `reportpreview.cpp` |
| `.pro.user` / `.qtcreator` 入库 | 删除并加入 `.gitignore` | `.gitignore` |
| 构建脚本 `$$TARGET.exe` 展开为空、windeployqt 静默失败 | 移除 post-link，部署独立为 `scripts/deploy.ps1` | `BDSMEP.pro` 注释 |
| `getCompleteness()` 恒 100% | 按中位数采样间隔推算应有历元数 | 测试 `trajectoryCompletenessReflectsGap` |
| GBK 编码探测写了却没用 | `TextFileReader` 接入解析链路 | `core/gnss/textfilereader.cpp` |
| 绘图把 quality=5（单点）当固定解 | 统一 `gnss::plotTypeOf`，5 归为单点 | 测试 `qualityNormalization` |

---

## 10. 已知限制

1. **GPST/UTC 不做互转**：以文件记录时间为准。若一侧是 GPST、另一侧是 UTC，会有 18 s 系统偏差（报告第五节会提示"匹配率过低"）。这是有意为之——静默转换比暴露问题更危险。
2. **表格格式按固定列位解析**（`[1]=时刻, [4..6]=BLH`），因为缺少格式规范；新格式需要扩展 `tabularparser.cpp`。
3. **表格源无解状态字段**，统一按固定解参与统计，解析日志与报告均会注明。
4. **报告不含图表**：HTML 目前是表格化文本；插入误差曲线/散点图是下一步。
5. **测速精度未实现**：数据源没有速度列，报告里显示 `-`。
6. **初始化时间**定义为"评估文件首个历元 → 首个固定解"，不区分冷/热启动。

---

## 11. 开发约定

- 新增指标：先改 `EvaluationResult`，再改 `ReportBuilder`（两处模板），最后补测试；不要在各处 `report +=`。
- 新增格式：只加 `gnss/xxxparser.cpp` 并在 `parsercommon.cpp` 注册，不在 UI 里写解析。
- core 不得 include widgets/ui；`tests` 只链接 core。
- 提交前跑 `scripts/test.ps1`，失败不提交。
