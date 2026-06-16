# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that renders GTA V graphics on screen.

**Rule:** Never stop. Update after each phase. Just work.

**Current Phase: 9 — GPU 命令翻译 → 真实游戏画面** ⚡ NEXT

---

## 已完成阶段

### Phase 1-8: 构建 → PPC 启动 → 崩溃修复 → Vulkan → 渲染循环 ✅

| Phase | 成果 |
|-------|------|
| 1-2 | SanRecomp.exe 编译/链接/运行 |
| 3 | XEX 加载 + PPC 入口执行 (0x83639888) |
| 4 | D3D12 紫色窗口 + 最小渲染 |
| 5 | PPC 启动诊断 — 零初始化内存根因 |
| 6 | 9 项内核补丁（空页哨兵、间接调用守卫、内存分配器）|
| 7 | 启动链补全：KeBugCheck 静默、TLS 递归打破、回调分发器覆盖 |
| 8 | **Vulkan 后端 + 渲染循环激活 + VdSwap 调用** |

### Phase 8 详细成果
- ✅ Vulkan 后端：`g_backend = Backend::VULKAN`
- ✅ vulkan-1.dll 已部署
- ✅ Intel D3D12 问题彻底绕过
- ✅ VBlank 60Hz → sub_822D41E8 → VdSetDisplayMode → VdSwap → Video::Present()
- ✅ 渲染循环完整链路打通
- ❌ 画面仍是蓝色/紫色——GPU 命令缓冲未被翻译为 Vulkan 绘制调用

---

## Phase 9: GPU 命令翻译 → 真实游戏画面

**问题：** 游戏调用了 VdSwap，画面在刷新，但显示的是清除色而非游戏内容。
**根因：** Xbox 360 GPU 命令（PM4 包/ring buffer）未被翻译为 Vulkan 绘制调用。
**目标：** 让游戏中的 3D 几何体、纹理、着色器真正渲染到窗口。

### 当前链路 vs 目标链路

```
当前：
VBlank → sub_822D41E8 → VdSwap → _VideoPresent() → plume Present → 蓝色/紫色

目标：
VBlank → sub_822D41E8 → GPU 命令 → Vulkan 绘制 → 纹理/几何体 → plume Present → 游戏画面
```

### 核心任务

1. **启用视频管线代码**
   - 解开 `gpu/video.cpp` 中 `#if 0` 的管线设置代码
   - 适配 Vulkan 后端（原代码为 D3D12 编写）

2. **GPU 命令缓冲翻译**
   - 实现 PM4 命令包的 Vulkan 翻译
   - 参考 `refs/UnleashedRecomp/UnleashedRecomp/gpu/video.cpp`（7500+ 行）
   - 参考 `refs/rexglue-sdk/src/graphics/vulkan/`（完整实现）

3. **着色器系统**
   - 运行 XenosRecomp 将 Xbox 360 .fxc → SPIR-V
   - 或复用 UnleashedRecomp 的着色器缓存

4. **纹理/资源加载**
   - 确保 RPF 资源提取正确
   - 确保纹理能传递到 Vulkan

---

## Phase Progress Log

### 2026-06-16 (Session — Vulkan 迁移 + 渲染循环)
- ✅ 研究 refs/ 所有项目渲染后端
- ✅ Vulkan 后端启用，D3D12 代码路径替换
- ✅ VBlank 60Hz 渲染驱动
- ✅ 找到 GTA V 渲染函数 sub_822D41E8
- ✅ VdSetDisplayMode + VdSwap 被游戏调用
- ✅ 渲染循环完整：VBlank → VdSwap → Video::Present
- ✅ Switch 移植路径确认（Plume Vulkan → NVK）
- ✅ rexglue wiki 学习 + 记录到 CLAUDE.md
- ✅ 共 38 commits 推送到 main

### Past sessions
- Phase 1-7: see git log for details
