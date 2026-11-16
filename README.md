# MiWear SDK

面向小米穿戴设备的实验性原生能力 SDK：提供页面、会话、线程、BLE 扫描/广播及固定通道 GATT 的固件适配与示例。项目聚焦可复用的底层接口、固件适配和生命周期管理。

Experimental native SDK for selected Xiaomi wearables and exact firmware profiles. Community project; not an official Xiaomi SDK.

## 支持范围

| 设备/组件 | 历史验收范围 | 当前边界 |
|---|---|---|
| Band11 NFC M2617B1 / 4.100.179，ABI1 / NS6 | 原生页面、线程/会话、扫描/广播、真实退出与 join | 固件专用适配；管理宿主仍驻留 |
| Band11，同固件，ABI3 / G4R2 | 前台 SOLO，手机蓝牙断开；Band central/GATT client，Windows peripheral/GATT server | 固定服务和特征；发送最多 20 字节；TX4/RX8 |
| S4 Sport M2312W1 / 3.111.032，ABI1 | NS6 原生基础与扫播验收 | 辅助测试设备；没有 S4 GATT 双端闭环结论 |

**ABI1 与 ABI3 分开构建和消费，不能直接混合链接。** 手机连接共存出现 error51，根因未确认；自动失败重连、后台/熄屏/唤醒、功耗和长期在线尚未完成。设备名中的 NFC 不代表已支持 NFC 读写。ATT 写响应和 notify 不等于业务送达确认。

上述是精确冻结基线的历史结果。本仓库重新构建的产物标记 hardware_tested=false，不自动继承真机 PASS。完整边界见 [当前能力与缺口](docs/当前能力与缺口.md) 和 [验收摘要](docs/验收摘要.md)。

## 开始使用

选定一条 ABI，先看对应公共头与最小源码示例：

- ABI1：[生命周期示例](sdk/abi1/examples/lifecycle.c)，公共头为 ns_session.h、ns_controller.h、ns_sdk_flow.h。
- ABI3：[固定通道示例](sdk/abi3/examples/fixed_channel.c)，公共头为 ns_link_v3.h；配套 [接口合同](sdk/abi3/docs/接口合同.md)。

示例需要匹配的宿主和固件后端，不能直接作为桌面程序运行。

只检查公开源码，不需要固件或设备：

```powershell
python tools/check_public.py
```

安装固定 Python 依赖后，可运行不依赖固件的 ARM 生命周期模型：

```powershell
python -m pip install --target work/python-deps -r requirements-build.txt
python tools/check_public.py --toolchain local-assets/zig --python-deps work/python-deps
```

完整构建还需要 Zig 0.13.0、Lua 5.4，以及由使用者自行提供的两份精确固件 AP。复制 config/local.example.json 为 config/local.json，填写本机路径后执行：

```powershell
python tools/integrate.py --mode public --config config/local.json
```

[离线构建说明](docs/离线构建.md) 包含依赖获取、哈希、完整流水线和本地历史回归的区别。固件、工具链、授权状态和设备包不随仓库分发；这里没有自动安装或刷机步骤。

本次干净副本构建与可复现性检查见 [首次公开源码验证](docs/开源准备.md)。

## 工程结构

- sdk/abi1、sdk/abi3：隔离的公共接口、核心、私有固件适配、宿主和模型。
- tools/：公开检查、离线构建、来源审计和源码导出工具。
- config/：外部依赖配置示例、基线哈希和公开文件白名单。
- migration/active-sources.json：逐文件冻结来源和原始哈希；不要求外部用户取得私有档案。
- docs/：[工程边界](docs/工程整合.md)、构建、验收及后续计划。

## 许可与贡献

自有部分采用 [MIT](LICENSE)。第三方 AGPL-3.0-only、MIT 与 Apache-2.0 部分详见 [THIRD_PARTY.md](THIRD_PARTY.md)。Band Lua 模板包含 Canopus AGPL 代码，完整组合不能称为纯 MIT。

贡献前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。适配新固件、改变宿主或 ABI 后，必须重新记录验证范围；不能扩大现有设备支持结论。后续方向见 [路线图](docs/路线图.md)。
