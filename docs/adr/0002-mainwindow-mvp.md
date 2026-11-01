# ADR 0002：MainWindow 改为被动视图（AppState + AppController）

- 状态：已采用
- 背景：MainWindow 曾达到 735 行 / 26 个函数 / 22 个成员，项目、导入、评估、
  报表、导航五类职责共享同一批字段。上一轮虽然拆出了 ProjectSession 和两个后台
  服务，但"什么时候清缓存、什么时候弹框、导入成功后切不切视图"仍散落在窗口类。

## 决定

1. **状态收敛为值对象** `AppState`（hasProject/files/busy/hasReport/...）。
2. **流程收敛到 `AppController`**（`src/services/appcontroller.*`）：
   - 拥有 ProjectSession / ImportService / EvaluationService；
   - 负责前置条件校验、文件复制、导入提交、评估编排、结果登记、报告缓存；
   - 只发信号：`stateChanged` / `progressChanged` / `statusMessage` /
     `logMessage` / `alertRequested` / `trajectoryReady` / `reportReady`；
   - 不依赖任何 QWidget，不弹对话框。
3. **MainWindow 只做被动视图**：
   - 用户操作 → 调用控制器方法；
   - 文件/目录选择对话框留在视图（选择文件是视图职责）；
   - `renderState()` 把 AppState 画到标题/任务树/表格/忙碌指示。
4. **控件内聚**：
   - `BusyIndicator` 收起状态栏进度条+取消按钮；
   - `FileTableView::setFiles(records)` 自己拼行，窗口不再构造 QStringList；
   - 树重建仍由窗口调用（记录数少，重建开销可忽略）。

## 目录调整（同批）

- `src/app/version.h` → `src/version.h`：单文件不值得一个目录；
- `src/utils/debugoutput.*` → `src/widgets/`：它是 QWidget，不是工具类；
- `src/utils/textcodechelper.*`、`textfilereader.*` → `src/core/gnss/`：只被解析器使用；
- `src/widgets/rtkplotwidget.*` → `src/widgets/plot/`：轨迹控件属于绘图管线；
- 删除 `src/utils`、`src/app`，目录从 7 个减到 5 个（core/services/ui/widgets + version.h）。

## 结果

| 指标 | 之前 | 之后 |
|---|---|---|
| `mainwindow.cpp` | 735 行 | 419 行 |
| `mainwindow.h` | 123 行 | 70 行 |
| MainWindow 成员 | 22 | 16 |
| 直接依赖的服务 | Session/Import/Evaluation | 仅 AppController |
| 无 GUI 可测的流程 | 0 | 项目/导入/评估/删除（控制器） |

419 行里主要是工具栏/分栏装配、文件对话框、树/报表的渲染与结果文件预览；
这些属于视图本身，不再继续外移，否则会变成"为了行数而拆"的转发层。

## 备选方案

- **控制器只发零散信号、不发 AppState**：窗口仍需维护多组 bool（busy/hasReport/...），
  就是旧的 22 个成员，放弃。
- **对话框也抽象成 DialogService**：能进一步减行数，但引入接口+实现两层，
  收益主要是可测试性；当前控制器已经无 GUI 依赖，暂不做。
- **把树/表格渲染再拆 Presenter**：会多一层只做转发的类，收益低，暂不做。
