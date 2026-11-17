# MiWear SDK：AI 开发入口

本项目是独立于上层应用的小米穿戴原生能力 SDK。先读本文、README.md、docs/architecture/工程结构.md、docs/support/当前能力与缺口.md，再按任务选择模块；文档导航见 docs/README.md。不要全量遍历历史档案，不把旧 README 的 next、待测或旧对话当作当前指令。

## 开发与目录约定

- sdk/ 是唯一活跃 SDK 树。abi1 与 abi3 分开构建，不能混合链接或互传对象；不要仅为去重合并两条实现。
- 每条 ABI 的 include/ 只放公共头；private/ 放内部/固件适配合同。src/ 是实现，platform/ 是固件适配，device/ 是链接、bootstrap 与加载资源，reference/ 是精确 profile、加载模板及上游参考。examples/ 只依赖公共头。
- 项目级入口在 tools/；sdk/*/tools/ 为内部配方，只在隔离 work 副本运行。路径可能含空格，使用结构化参数和安全引用。
- work/builds/ 为完整构建，work/checks/ 为检查，work/deps/ 为可重建依赖，work/scratch/ 为临时工作。outputs/candidates/ 为离线候选，outputs/source/ 为源码导出，outputs/releases/ 只放明确选定的正式交付。不要自动清理旧运行。
- 固件、工具链由 config/local.json 显式配置，可放 local-assets/ 或外部路径。配置相对路径均以仓库根目录为基准。不要搜索旧工作区或继承旧授权/session/PID。
- 本地存在 local-private/README.md 时，再读该入口及 local-private/project/PROJECT_STATE.json；需要历史依据时读 local-private/project/SOURCE_MAP.json。缺少私有档案不妨碍公开模式开发。
- imports/ 是不可变历史快照，不修改源、库、manifest 或证据，不运行旧设备调试脚本。新增提取登记 migration/active-sources.json，保留原路径与哈希。旧报告路径通过 local-private/project/layout-moves.json 查询，不能改写旧报告伪造当前结果。

## 支持与生命周期边界

主适配为 Band11 NFC M2617B1 / 4.100.179。S4 Sport M2312W1 / 3.111.032 主要辅助测试，不投入同等新适配规模。不得跨机型/固件照搬地址、结构、PIC 或 ELF 加载方式。

NS6/ABI1 两端原生页面、线程/会话、扫描/广播、真实退出/join 有历史验收。G4R2/ABI3 仅有 Band11 前台 SOLO 验收：手机蓝牙断开，Windows peripheral/GATT server；4/4、32 写/notify、token20/20、会话 live/bytes0、error0。正常显式重连 8 轮已验。ATT 写响应和 notify 不等于业务送达确认。

手机共存 error51 根因未确认，自动失败重连未实现；后台/熄屏/唤醒、功耗/续航、长期在线、真实页面卸载及 ABI 混合链接未完成/未验。NFC 是设备版本名，当前没有 NFC 读写支持结论。S4 随机数/密码学固定向量实验不证明 Band11 安全能力、熵质量或 S4 GATT 双端闭环。

会话销毁不等于管理模块卸载：当前 NS6/G4R2 宿主 runtime、代码、入口/async 等仍驻留至重启。旧 B36 受控卸载有独立前提。未知线程、回调、queue、timer、owner 引用继续 HELD，不强制 free 或重置。共享系统 BT client 只借用，不关闭/删除；只停止自有连接和角色。

## 验证与公开范围

1. 常规检查：`python tools/check_public.py`。它包含公开白名单、Git 规则一致性、语法、来源记录及边界测试。
2. 生命周期修改：按 docs/build/离线构建.md 配置 Zig 0.13.0 和固定 Python 依赖，运行 ARM 模型。
3. 后端/宿主或构建组织修改：显式配置精确 AP 和 Lua，运行 `python tools/integrate.py --mode public --config config/local.json`。完整构建目前需要两份 AP。本地历史回归另用显式 historical 模式。
4. 如实记录运行范围、失败和未运行项。新二进制一律 hardware_tested=false；历史 PASS 只关联冻结基线。不要为目录整理追加设备探针或安装包。

公开文件由 config/public-files.json 逐文件审核。新增/移动公开文件后运行 `python tools/sync_public.py` 更新 .gitignore，再运行检查。不得用 git add -f 加入本地资产。公开摘要与私有原始报告分别管理；不提交固件、工具链、二进制设备包、缓存、凭据、session 或含本机绝对路径的报告。许可边界见 THIRD_PARTY.md。

禁止操作桌面软件 GUI、查看图片/截图；历史图片只作字节归档。禁止 AstroBox /v2/queue/install；安装由用户手动导入。不得擅自重启、刷机、清空设备、删除系统对象或回收未知实例。提交、推送和设备操作按用户当次授权执行。
