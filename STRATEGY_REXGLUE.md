# STRATEGY: Incremental rexglue-sdk Adoption

> **状态：** 策略评估阶段  
> **日期：** 2026-06-17  
> **背景：** 当前 XenonRecomp 方案的内核补丁已达极限 — 游戏自然图形初始化无法完成

---

## 为什么不继续修补 XenonRecomp

当前方案已有 50+ 变通方法（见 `DEVELOPMENT_HACKS.md`）。核心问题：

1. **忙等破解器脆弱** — 轮换值靠运气推进状态机，不同初始化阶段需要不同值
2. **内核服务不完整** — 线程创建、事件、定时器虽已实现但游戏不用（使用软件自旋锁）
3. **图形状态未知** — `0x83830000` 处的结构从未被正确初始化
4. **D3D 初始化链崩溃** — `sub_82989588` 需要大量预初始化结构

**每个新发现的缺失函数 → 新增变通方法 → 更多维护负担**

rexglue-sdk 的方案则从根源解决：完整的内核实现让游戏自然完成所有初始化。

---

## rexglue-sdk 现状

预编译工具：`refs/rexglue-sdk/rexglue-bin/win-amd64/bin/rexglue.exe`  
源代码：`refs/rexglue-sdk/`  
已生成的 GTA V 代码：`refs/rexglue-sdk/rexglue-bin/win-amd64/test_gta5/generated/default/`

| 项目 | 数值 |
|------|------|
| 生成的 .cpp 文件 | 197 个 |
| 函数映射条目 | ~47,000 |
| 入口点 | `xstart` (0x83639888) |
| 映像基址 | 0x82000000 |
| 映像大小 | 0x1DC0000 (31MB) |
| 代码基址 | 0x82210000 |
| 代码大小 | 0x15584F4 (~22MB) |

---

## 增量采用计划

### 阶段 1：构建验证 ✅ → NEXT GOAL

**目标：** 编译 rexglue-sdk 静态库 + 验证 GTA V 生成的代码可编译

**任务：**
- [ ] 1.1 配置 rexglue-sdk CMake 构建（Ninja + clang-cl）
- [ ] 1.2 编译 rexglue-sdk 核心库（rexglue_core, rexglue_kernel, rexglue_graphics）
- [ ] 1.3 创建最小测试项目，链接 rexglue 库 + 生成的 GTA V .cpp 文件
- [ ] 1.4 验证编译通过，记录需要修复的问题
- [ ] 1.5 如果编译成功，尝试加载 XEX 并运行到入口点

**验证标准：** 至少一个 rexglue 子库编译成功，GTA V 生成的 .cpp 文件无编译错误

### 阶段 2：内核替换

**目标：** 用 rexglue 内核替换当前 `imports.cpp` 中的内核实现

**任务：**
- [ ] 2.1 列出 rexglue 内核导出的所有函数（对比当前 imports.cpp）
- [ ] 2.2 将 rexglue 内核模块（xboxkrnl, xam, xbdm）链接到 SanRecomp
- [ ] 2.3 移除对应的存根和变通方法
- [ ] 2.4 测试游戏初始化（预期：无需忙等破解器即可完成）

**验证标准：** 游戏 `_xstart` 返回且无需忙等破解器

### 阶段 3：图形集成

**目标：** 用 rexglue 图形管道替换当前的 PrepareFrameAndPresent

**任务：**
- [ ] 3.1 了解 rexglue 的 PM4→Vulkan 翻译架构
- [ ] 3.2 将其图形管道集成到我们现有的 plume/Vulkan 基础设施中
- [ ] 3.3 测试游戏渲染（预期：窗口中出现实际游戏图形）

**验证标准：** 窗口中可见游戏图形（非纯色清除）

### 阶段 4：PPC 代码切换

**目标：** 将 XenonRecomp 生成的 PPC 代码替换为 rexglue 生成的代码

**任务：**
- [ ] 4.1 用 rexglue 的 197 个文件替换 SanRecompLib 的 546 个 XenonRecomp 文件
- [ ] 4.2 适配函数映射和分派表差异
- [ ] 4.3 移除所有 XenonRecomp 特有的变通方法
- [ ] 4.4 全面测试

**验证标准：** 游戏完整启动并通过 Vulkan 渲染图形

---

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| rexglue-sdk 无法编译 | 使用其预编译二进制文件；调整编译标志 |
| rexglue 生成的 PPC 代码与当前不同 | 阶段 4 之前先进行编译测试 |
| 图形集成复杂 | 保留当前 Vulkan 基础设施，仅替换内核 |
| 与现有代码的链接冲突 | 逐步替换，每次只处理一个组件 |

---

## 决策点

**阶段 1 完成后决策：** 如果 rexglue-sdk 库能成功编译并与 GTA V 生成的代码链接，则继续阶段 2。如果失败，回退到修补 XenonRecomp/内核的方法。

*最后更新：2026-06-17*
