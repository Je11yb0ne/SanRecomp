# Task Plan — SanRecomp GTA V 重编译

**目标：** GTA V Xbox 360 → PC，用 rexglue 预编译 SDK

**当前阶段：** rexglue 正确集成（Wiki 模式）

**分支：** `feature/rexglue-source-build`

---

## 当前状态

| 项目 | 状态 |
|------|------|
| rexglue 预编译 SDK | ✅ tools/rexglue-sdk-0.8.1.32-dev |
| rexglue Wiki 已读 | ✅ refs/rexglue-sdk-wiki/ |
| 197 PPC 文件编译 | ✅ test_rexglue/generated/default/ |
| GTA5App (ReXApp) | ✅ 简化为 TDURE 模式 |
| CMake (find_package) | ✅ 链接到 rex::runtime |
| 构建 | 🔧 编译中 |

## 下一步

1. 构建成功 → 部署 rexruntimerd.dll → 测试启动
2. 游戏启动后，rexglue 自动处理内核/GPU/音频/输入
3. 验证游戏画面（rexglue 有完整 GPU 仿真）
