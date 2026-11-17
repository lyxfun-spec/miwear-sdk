# 独立应用 SDK 开发候选验证（2026-11-17）

本轮仅修改 SDK，没有开发 APP 或进行设备操作。所有新候选 hardware_tested=false。本摘要不含私有原始验收记录、本机路径或受限二进制；源码提交和推送不代表设备验收或正式二进制发布。

## 改进与入口

- ABI1 公共头 sdk/abi1/include/ns_ble.h，实现在 src/ns_ble_service.c，复用 ns_sdk_ble_backend.c；接口合同 sdk/abi1/docs/接口合同.md。
- 新增 open/start/change/poll/receive/observe/stop/exit：31 字节完整 AD 复制、单待发帧背压、RX8 值复制、溢出计数、应用 owner 与显式退出、10 秒 polled drain 超时 HELD。
- 广播内容更新通过原后端停止所有角色、完整回收、重开实现；启动间隔至少 1000ms，有扫播间隙，没有热更新 RPC 或送达保证。
- sdk/abi1/examples/ble_service.c 仅用公共头；两设备库分别编译、链接该示例。原三个生命周期公共头保持原行为。
- ABI3 fixed_channel 示例增加显式 stop、重用前状态重置和发送失败关闭；断开后默认 close，重连意图由应用管理。公共头/合同明确 worker 内部 500ms backoff，传输核心未修改。
- docs/architecture/独立应用接入.md 记录 ABI 协作方案、GATT Server 前提、error51 分组复现计划和双穿戴 GATT 缺口。未合并 ABI。
- 公共白名单、Git 选择、头文件导出、公共示例构建和模型入口同步更新。

## 实际验证

| 检查 | 结果与范围 |
|---|---|
| python tools/check_public.py | PASS：公开选择、Python/JSON 语法、来源分类、发布边界、验收解析 |
| 配置 Zig 0.13.0 和固定 Python 依赖的 check_public | PASS：原 ARM 生命周期 13 场景、222 断言；新增服务模型 70 断言，failure_line=0 |
| python tools/integrate.py --mode public --config config/local.json | PASS：19 阶段，含两设备 ABI1 库/宿主/示例、ARM/UI/ROM loader/重定位、ABI3 库/宿主/ROM/资源/SDK 模型、公共示例及 Lua fixture |
| ABI 混合链接负测试 | 37 个重复定义；链接按预期拒绝，未绕过冲突 |
| 冻结字节比较 | 6/12 相同；本轮变更导致其他产物变化，如实记录，未改写基线哈希 |
| 修改差异空白检查 | 按既有 CRLF 字节约定设置 cr-at-eol 后通过 |

新增 ARM 测试在 sdk/abi1/tests/ble_service_contract.c：覆盖固件 guard 拒绝、1/31/32/0 长度、NULL、AD 截断/填充、scan-only 参数、TX/RX 复制、单待发帧背压、RX 满队列/空队列/计数器回绕、UI 上下文拒绝、毫秒回绕/逆行、停止与退出、无真实退出证据不 join、无 queue/timer 清理证明不释放、超时 HELD 后不重启/释放，以及两设备 packet 编码/报告长度边界。

服务模型使用生产 session/controller、公共服务与 packet 代码，固件线程/回调调度和清理证据由 mock 提供；既有后端模型仍走原固定标记。没有把“公共动态帧经模型通过”写成“新无线行为实机通过”。首次完整构建暴露静态存储不符合 raw PIC 的仅 .text 要求，已改为运行时分配驻留服务，最终全流程通过。失败运行保留，没有清理或覆盖历史记录。

本轮最终完整运行标识 integration-85kv_t9f；其 work/builds 下 run.json、logs 和模型报告仅保存在本机，候选在 outputs/candidates 同名目录。不公开含绝对路径的原始报告。公开模式 frozen_origins_verified=false；未运行 historical 模式、同一最终输入的 --compare 重现检查或新真机验收。

## 使用判断与待验项

可以基于精确固件/匹配常驻宿主开发前台原型。ABI1 可用于广播发现和短数据实验；ABI3 可用于固定 GATT Client 对已有 peripheral/server。应用 owner 跨页面持有服务，页面不拥有通信生命周期，退出继续驱动清理。

新增动态广播/RX 需另行授权的实机验收；广播换帧有间隙、没有可靠传输或设备身份接口。GATT Server、穿戴双端闭环、跨 ABI 协作、error51 根因及失败自动重连仍缺失；自然熄屏、系统挂起/唤醒、功耗、长期在线和模块卸载未验证。当前不能据此承诺完整离线蓝牙聊天产品的可用性。
