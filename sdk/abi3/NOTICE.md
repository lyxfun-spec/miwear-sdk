# 来源与许可

本目录从项目冻结的 G4R2 基线选择性提取。port、scan packet、Band loader 模板和 facebin 最初来自 native-session-engineering。逐文件来源及 SHA256 见项目根 migration/active-sources.json；原 baseline-audit 与验收 evidence 由 SOURCE_MAP.json 的 gatt_source/gatt_acceptance 索引，未复制为本轮新证据，也不改原工程。

Band Lua模板嵌入Canopus recover_execute，固定来源树 `1f8b38b7acbce9ac22fe0eacf88283a91e6abed0`，AGPL-3.0-only；完整模板源码及LICENSE-Canopus-AGPL-3.0-only.txt随本工程提供。沿用该组合的交付须遵循其许可义务。facebin源码及MIT许可证在vendor目录。

reference/upstream中的open-vela参考文件保留原Apache-2.0声明；固定commit、原路径及SHA256见该目录provenance.json。它们用于语义核对，并非把上游头文件当作零售固件的已开放合同。

新增G1实验源码、构建/验证工具均随设备包提供。固件不打包；static-audit只记录与本接口相关的窗口指令、哈希、对象布局和证据边界。没有将模型或电脑冒烟日志标记为手环真机验收。
