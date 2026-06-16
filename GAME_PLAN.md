# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that boots the game with Vulkan rendering.

**Rule:** Never stop. Update after each phase. Just work.

**Current Phase: 8 — Vulkan 渲染 → 游戏画面** ⚡ EXECUTING
**Goal Progress: 4/5 criteria met**

---

## 已完成阶段

### Phase 1-7: 构建、渲染、PPC 启动、内核补丁 ✅
- SanRecomp.exe 编译/链接/运行，Vulkan 后端
- D3D12 紫色窗口（Intel 驱动问题）
- XEX 加载 + PPC 入口执行
- 所有崩溃修复（忙等、栈溢出、KeBugCheck、间接调用）
- VBlank 60Hz 渲染驱动运行中

### Phase 8: Vulkan 渲染后端 ✅ (进行中)

**已达成：**
- ✅ Vulkan 后端（g_backend = Backend::VULKAN）
- ✅ vulkan-1.dll 部署
- ✅ Intel D3D12 问题完全绕过
- ✅ VBlank 60Hz 定时器运行
- ✅ 游戏渲染循环已激活（VdSetDisplayMode 被调用）
- ✅ 渲染函数 sub_822D41E8 被找到并通过 VBlank 调用
- ✅ PCR 上下文已保存用于 VBlank 回调
- ✅ rexglue wiki 学习完毕并记录
- ✅ Switch 移植路径确认（Plume Vulkan → NVK）

**进行中：**
- 🔄 渲染函数运行中（LR=0x836DECA8），需要到达 VdSwap

---

## Goal 验证标准

| # | 标准 | 状态 |
|---|------|------|
| 1 | LR 跳出 0x836411E0 | ✅ 多样化 LR (0x836267B8-0x836DECA8) |
| 2 | INDIRECT-CALL 大幅减少 | ✅ 接近零 |
| 3 | KeBugCheck/KeBugCheckEx 消除 | ✅ 0 次出现 |
| 4 | 渲染函数被调用 | ✅ VdSetDisplayMode 已调用 |
| 5 | 窗口出现游戏画面 | ⚠️ 渲染循环运行中，VdSwap 待触发 |

---

## Phase Progress Log

### 2026-06-16 (Session — Vulkan 迁移 + 渲染)
- ✅ Vulkan 后端启用（研究 refs/ 所有项目）
- ✅ sub_822D41E8 渲染函数找到
- ✅ VBlank 定时器默认回调设置
- ✅ VdSetDisplayMode 被调用——渲染循环验证
- ✅ PCR 上下文保存用于 VBlank 回调
- ✅ Switch 路径：Plume Vulkan → Mesa NVK → Tegra X1

### 下次继续
- 修复 VBlank 回调上下文使渲染函数到达 VdSwap
- 完整的 D3D12→Vulkan 管线转换
- Switch 交叉编译工具链设置

### Past sessions
- Phase 1-7: see git log for details
