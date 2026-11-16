# 第三方代码与许可

自有代码和文档按根目录 LICENSE 的 MIT 许可发布。下列第三方文件保留各自许可和原作者声明；根目录 MIT 不对这些文件重新授权。

| 文件范围 | 来源 | 许可与随附文本 |
|---|---|---|
| sdk/abi1/reference/band_loader.lua.in、sdk/abi3/reference/band_loader.lua.in | 嵌入 Canopus recover_execute；记录的来源树为 1f8b38b7acbce9ac22fe0eacf88283a91e6abed0 | AGPL-3.0-only；完整文本分别位于两条 ABI 目录的 LICENSE-Canopus-AGPL-3.0-only.txt（ABI1 位于 reference/） |
| sdk/abi3/vendor/facebin.py | 历史工程携带的 facebin；原声明 Copyright (c) 2026 air | MIT；同目录 LICENSE-facebin.txt |
| sdk/abi3/reference/upstream/ 中的 C/H 文件 | open-vela 语义参考；原作者声明保留于各文件，原路径和 SHA256 见 provenance.json | Apache-2.0；同目录 LICENSE |

Band Lua 模板包含第三方 AGPL 代码，沿用该组合的分发仍须遵守其许可；不能把带模板的完整工程或设备包描述成纯 MIT。纯原生公共接口与自有实现的许可范围按上述文件划分。

本次开源保留已有第三方声明，未对第三方取得额外授权。早期上游参考记录中，部分条目只有路径和哈希，没有 commit 或仓库 URL；不补造这些信息。冻结来源以 migration/active-sources.json 为索引；其中 imports/ 路径是维护者私有历史档案标识，档案不随仓库分发。

Zig、Python、pyelftools、Unicorn 和 Lua 是外部构建/验证依赖，不随此源码仓库打包。依赖应从各自上游取得，其许可不由根目录 MIT 替代。Apache 许可文本来源：https://www.apache.org/licenses/LICENSE-2.0.txt。
