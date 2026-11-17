# 活跃 SDK 代码

自有部分按根目录 MIT 许可发布；第三方文件范围见根目录 THIRD_PARTY.md。下文 SOURCE_MAP 是维护者的私有历史索引，不随公开源码提供；外部用户可使用 migration/active-sources.json 中的记录和公开验收摘要。

本目录是唯一活跃树。`abi1` 与 `abi3` 的公共合同、私有后端和宿主隔离保留；从项目根目录执行 `python tools/integrate.py`。内部构建脚本只在 work 中的副本执行，实际能力与未验证项见根目录 docs。

ABI1 来源为 NS6 工程和其冻结 ABI1 源码，ABI3 来源为 G4R2 与 SDK0.3.0 前台 SOLO 交付。生产源码保留原始语义和所有权证明；新测试运行器只用于离线模型。每文件来源/原哈希见 `migration/active-sources.json`，新文件由审计工具单独列出。未来改动在这里进行，不能回写 imports。

两条 ABI 均以 `include/` 作为公共头目录，`private/` 作为内部头目录。ABI1 公开三个生命周期头及 ns_ble.h 数据服务候选，ABI3 公开 ns_link_v3.h；消费者示例只依赖公共头。内部构建才加入 private/ 搜索路径。完整职责见根目录 docs/architecture/工程结构.md；历史来源索引现位于 local-private/project/SOURCE_MAP.json。

Canopus Lua recover_execute 模板为 AGPL-3.0-only，许可在 `abi1/reference/` 和 `abi3/`；facebin MIT 许可在 `abi3/vendor/`；open-vela 语义参考保留 Apache-2.0 声明和 provenance。以上不是对全部项目源码重新授权。原始 NOTICE 与研究资产继续由 SOURCE_MAP 追溯。
