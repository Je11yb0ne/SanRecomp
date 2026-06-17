# STRATEGY: rexglue-sdk 集成方案

> **状态：** 准备启动 — 已有预编译 SDK v0.8.1.32-dev  
> **日期：** 2026-06-17

---

## 决策：使用预编译 rexglue-sdk

SDK 位置：`tools/rexglue-sdk-0.8.1.32-dev.gf22cd9d-win-amd64/`

### 为什么选 rexglue

| 对比项 | XenonRecomp (当前) | rexglue-sdk |
|--------|-------------------|-------------|
| 内核完整性 | ⚠️ 50+ 存根 + 忙等破解器 | ✅ 完整内核（线程、事件、MMIO、内存） |
| 图形 | ⚠️ 自制 Vulkan 清除色 | ✅ 内置 Vulkan 后端 + PM4 命令处理器 |
| PPC 文件 | 546 个 | 197 个（可重新生成） |
| 音频 | ⚠️ FFmpeg 存根 | ✅ libavcodec/libavutil 预编译 |
| 输入 | SDL2 | SDL3 预编译 |
| 构建 | 从源码编译 | **预编译 .lib 文件** |
| 维护负担 | 高（每个缺失函数 → 新 hack） | 低（完整 API 实现） |

### 预编译 SDK 清单

```
tools/rexglue-sdk-0.8.1.32-dev/
├── bin/
│   ├── rexglue.exe          ← 代码生成器
│   ├── rexruntime.dll        ← 运行时 DLL (release)
│   ├── rexruntimed.dll       ← 运行时 DLL (debug)
│   └── rexruntimerd.dll      ← 运行时 DLL (relwithdebinfo)
├── lib/
│   ├── rexruntime.lib        ← 核心运行时 (6MB)
│   ├── SDL3-static.lib       ← 窗口/输入 (8MB)
│   ├── libavcodec.lib        ← FFmpeg 音频解码 (1.3MB)
│   ├── libavutil.lib         ← FFmpeg 工具 (1MB)
│   ├── fmt.lib, spdlog.lib   ← 日志
│   ├── snappy.lib, xxhash.lib ← 压缩/哈希
│   └── o1heap.lib            ← 确定性内存分配器
├── include/
│   ├── rex/                  ← 核心 API 头文件
│   │   ├── runtime.h         ← Runtime::Setup() 入口
│   │   ├── kernel/           ← 内核模块 (xboxkrnl, xam, xbdm)
│   │   ├── graphics/         ← D3D12 + Vulkan 后端
│   │   │   └── vulkan/       ← 完整 Vulkan 命令处理器 + PM4 翻译!
│   │   └── system/           ← 线程、事件、内存、定时器
│   ├── SDL3/                 ← SDL3 头文件
│   └── imgui.h               ← ImGui
└── cmake/                    ← SDL3 cmake 配置
```

---

## 集成计划

### 阶段 1：代码生成 + 编译验证 ← NEXT GOAL

**目标：** 用新版 rexglue 重新生成 GTA V PPC 代码，创建最小项目链接预编译库

- [ ] 1.1 用 `rexglue.exe codegen` 重新生成 GTA V 的 PPC→C++ 代码
- [ ] 1.2 创建 `test_rexglue/` 目录，包含最小 CMakeLists.txt
- [ ] 1.3 链接：rexruntime.lib + SDL3-static.lib + 依赖
- [ ] 1.4 包含生成的 .cpp 文件 + rex 头文件
- [ ] 1.5 编译通过，记录所有编译错误及修复
- [ ] 1.6 (如果编译通过) 运行并验证入口点可达

**验证：** `test_rexglue.exe` 编译成功，链接无错误

### 阶段 2：内核替换

**目标：** 用 rexglue 内核替换 SanRecomp 的 imports.cpp

- [ ] 2.1 从 SanRecomp 中移除内核存根/变通方法
- [ ] 2.2 链接 rexruntime.lib（内核 + 线程 + 内存）
- [ ] 2.3 链接 SDL3-static.lib（输入）+ libavcodec.lib（音频）
- [ ] 2.4 测试游戏初始化（预期：无需忙等破解器）

**验证：** `_xstart` 返回，无 BUSY-WAIT 消息

### 阶段 3：图形集成

**目标：** 用 rexglue 图形管道处理实际渲染

- [ ] 3.1 使用 `rex::graphics::vulkan::VulkanGraphicsSystem` 替代我们的 PrepareFrameAndPresent
- [ ] 3.2 rexglue 的 PM4 命令处理器自动翻译游戏绘制调用
- [ ] 3.3 保留我们的 plume 基础设施作为回退

**验证：** 窗口中可见游戏图形

### 阶段 4：PPC 代码切换

**目标：** 用 rexglue 生成的 197 个文件替换 SanRecompLib

- [ ] 4.1 移除 XenonRecomp 生成的 546 个文件
- [ ] 4.2 使用 rexglue 生成的代码及其函数分派
- [ ] 4.3 移除所有 XenonRecomp 特有的变通方法
- [ ] 4.4 全面测试

**验证：** 游戏完整启动并通过 Vulkan 渲染

---

## 命令行参考

```bash
REXGLUE="tools/rexglue-sdk-0.8.1.32-dev.gf22cd9d-win-amd64/bin/rexglue.exe"
REX_SDK="tools/rexglue-sdk-0.8.1.32-dev.gf22cd9d-win-amd64"

# 重新生成 GTA V 代码
"$REXGLUE" codegen gta5_config.toml -v --force

# 编译时链接
# -I"$REX_SDK/include"
# -L"$REX_SDK/lib"
# -lrexruntimerd -lSDL3-staticrd -llibavcodecrd -llibavutilrd
# -lfmtrd -lspdlogrd -lsnappyrd -lxxhashrd -lo1heaprd
```

---

## 决策点

- **阶段 1 后：** 编译成功 → 继续阶段 2。编译失败且无法修复 → 回退到修补 XenonRecomp
- **阶段 2 后：** 游戏自然初始化 → 继续阶段 3。初始化仍卡住 → 需要更多调试

---

*最后更新：2026-06-17 (更新为预编译 SDK 方案)*
