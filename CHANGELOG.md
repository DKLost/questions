# 变更日志

## v0.6.0 — 2026-11-07

知识图谱内核落地后的首个版本：从旧式索引树迁移到「谓词三元组 + 派生链名」的现象树，并补齐复习日志、设置与统计闭环。

### 新增

- **知识图谱内核**：现象以「谓词 主词 宾词」三元组存储，支持派生链名 `A→pred:[B]`、指象/具象、`is-a` 锚点与合并行；知识树改为批量建树、零写库加载。
- **三区界面**：知识索引区、题目表、题面与答案区；删除旧式索引树与分类体系。
- **复习系统**：遮罩、门控、乱序池、四档自评；单项/表项/待学的复习入口；当前空、当前题、当前过滤表的提前复习。
- **题面与答案**：HTML 与 cloze 编辑、Typst 公式实时编译与图片缓存、答案迁移/绑定、题目改名与改挂。
- **检索与浏览**：概念子树范围过滤（段边界精准匹配）、顶层宾词支自动并入、宾词视图、复习界面反选兜底、可并排的题目预览窗池。
- **复习日志**：新增 `review_log` 表，按 FSRS 社区 schema 记录 `card_id / review_time / review_rating / review_state / review_duration`。
- **FSRS optimizer 支持**：工具栏「工具 → 导出复习日志」输出 `revlog.csv`，可直接运行 `python -m fsrs_optimizer revlog.csv`。
- **设置**：FSRS 21 个权重（支持粘贴 optimizer 输出、恢复默认）、目标保留率、最大间隔、easy/hard 用时阈值、新卡估时系数、Typst 可执行文件路径、导入前自动备份与保留份数。
- **统计工具栏**：待学、今日复习、今日新学（含复习失败后重学）、今日学习用时；不做正确率与连续天数。
- **数据安全**：parser 导入前自动备份 `question.db` 与 `knowledge.db`，按份数滚动清理。

### 修复

- 门控批量记账的重复写、漏写与揭示刷红：同一空在一批内只写一次，兄弟代答不再被自己的空记录判错。
- 改名后右侧编辑区无法添加或修改答案（`submitAll()` 后按 id 回选本题）。
- 无选中题目时 html/cloze 编辑区未置灰，过滤后仍显示上一题题面。
- 题目过滤由子串匹配改为段边界精准匹配，避免 `定义` 误命中 `定义域`。
- 改挂/迁移/复习后知识树「到期」列不刷新的问题。
- Typst 公式与文本对照的像素级错位。

### 变更

- 工程目录重构为 `core / parser / widgets / dialogs / services / utils`。
- FSRS 升级并固定为 v6.1.1 的本地实现，排期、评级与复习日志共用同一套状态机。
- 界面状态继续存于 `ui_state.ini`；用户设置独立存于 `settings.ini`。
- 实验性 `prednet/` 保留在仓库中，不参与主程序构建。

### 升级说明

- 旧 `question.db` / `knowledge.db` 可直接打开；启动时自动创建 `review_log` 表与索引。
- `review_log` 从本版本开始记录，历史复习无法回溯，optimizer 需要本版本之后积累的数据。
- 本版本内部只有 `new / learning / review` 三态，日志中的 `review_state` 为 `0 / 1 / 2`，不区分 Relearning。
- 裸 `questions.exe` 需要 Qt 6.11.2 MinGW 运行时；发布包已携带 Qt DLL 与插件，`typst.exe` 也已放入包内（仍可在设置中指定其他路径）。

### 构建

```bash
qmake ../questions.pro
mingw32-make -f Makefile.Release -j4
```

产物：`build/release/questions.exe`（约 1.1 MB，已 strip）。

发布包（含 Qt 运行时、SQLite 驱动与 typst.exe）：

```bash
windeployqt --release --no-translations --no-system-dxc-compiler --compiler-runtime questions.exe
```

本项目是 Qt Widgets 应用，不需要 D3D12 的 `dxcompiler.dll` / `dxil.dll`，因此跳过 DXC 部署；`D3Dcompiler_47.dll` 仍会保留。


### 已知限制

- 仅验证 Windows + Qt 6.11.2 MinGW 构建。
- 复习日志、今日统计与 optimizer 数据均从本版本开始积累。
- 复习排期以本地日期为日界，暂不支持自定义“一天开始时间”。
