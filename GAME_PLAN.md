# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that boots the game with Vulkan rendering, portable to Switch.

**Rule:** Never stop. Update after each phase. Just work.

**Current Phase: 8 — Vulkan 渲染后端 → 完整画面渲染** ⚡ EXECUTING

---

## 已完成阶段

### Phase 1-7: 构建、渲染、PPC 启动、内核补丁 ✅
- SanRecomp.exe 编译/链接/运行
- D3D12 紫色窗口（最小模式，完整管线在 Intel 上 #if 0）
- XEX 加载 + PPC 入口执行
- 所有崩溃修复（忙等、栈溢出、KeBugCheck、间接调用）
- VBlank 60Hz 渲染驱动运行中
- 渲染循环活跃（VBlank → Video::Present）

---

## Phase 8: Vulkan 渲染后端（方案 A：Plume Vulkan）

**主线：** 给 SanRecomp 加 Vulkan 后端，参考 UnleashedRecomp 的 Plume 双后端实现。
**目标：** Intel GPU 上完整渲染管线工作，显示游戏画面。
**未来：** 同样的 Vulkan 后端直接用于 Switch 移植（NVK）。

### Step 8.1: 启用 Plume 的 Vulkan 后端

**目标：** 编译 Plume 的 Vulkan 静态库，链接到 SanRecomp.exe。

```
验证：cmake --build 成功，Plume Vulkan 后端编译无错误
```

#### 8.1.1 检查 Plume Vulkan 依赖
- volk (Vulkan loader)
- Vulkan SDK / headers
- SPIR-V 相关 (glslang, SPIRV-Tools)
- 检查 thirdparty/plume 的 CMakeLists.txt 中的 Vulkan 编译选项

#### 8.1.2 配置 CMake 启用 Vulkan
- 类似 UnleashedRecomp：设置 `PLUME_VULKAN=ON`
- 链接 Vulkan 库
- 确保 D3D12 和 Vulkan 可以同时编译

#### 8.1.3 编译验证
- Full rebuild with Vulkan enabled
- Fix any missing dependencies

### Step 8.2: 添加 Vulkan 渲染路径到 video.cpp

**目标：** 参考 UnleashedRecomp 的 video.cpp，添加 Vulkan 设备创建和渲染路径。

```
验证：SanRecomp.exe 启动，Video 设备创建成功（Vulkan 后端）
```

#### 8.2.1 研究 UnleashedRecomp 的 Vulkan 实现
- `refs/UnleashedRecomp/UnleashedRecomp/gpu/video.cpp`
- 关键点：
  - Vulkan 设备创建（`CreateVulkanDevice`）
  - Vulkan swap chain 创建
  - 着色器加载（SPIR-V）
  - ImGui 的 Vulkan 后端
  - Intel GPU 自动检测和回退

#### 8.2.2 移植 Vulkan 设备创建
- 添加 `CreateVulkanInterface()` 调用
- 添加 Vulkan 设备枚举（物理设备选择）
- 添加 Vulkan swap chain 创建

#### 8.2.3 移植渲染命令翻译
- Vulkan command buffer 录制
- Pipeline state 管理
- Descriptor set 管理

#### 8.2.4 Intel GPU → 自动选择 Vulkan
- 添加 GPU 厂商检测
- Intel → 优先使用 Vulkan
- AMD/NVIDIA → 可配置

### Step 8.3: 完整渲染管线测试

**目标：** 游戏画面渲染到窗口（不只是紫色清除色）。

```
验证：窗口中出现游戏画面
```

#### 8.3.1 着色器编译
- 运行 XenosRecomp 转换 .fxc → SPIR-V（不是 DXIL）
- 或使用现有 UnleashedRecomp 的着色器缓存

#### 8.3.2 VBlank 驱动完整渲染
- 确保 VBlank → 游戏回调 → GPU 绘制 → Present 完整链路工作

### Step 8.4: Switch 移植准备（未来）

**目标：** 代码结构准备好编译到 Switch。

```
验证：代码使用 Plume Vulkan 抽象，无 Windows 特定依赖
```

#### 8.4.1 分离平台特定代码
- 窗口创建：`#ifdef __SWITCH__` → libnx, else → SDL
- 输入：Switch HID driver 参考 UnleashedRecomp-NX

#### 8.4.2 准备 NVK 链接
- 参考 `refs/UnleashedRecomp-NX/patches/plume.patch`
- 添加 `VK_USE_PLATFORM_VI_NN` 支持

---

## Phase Progress Log

### 2026-06-16 (session — 渲染管线研究 + Vulkan 计划)
- ✅ 研究 refs/ 下所有项目的渲染后端
- ✅ 确认 Intel D3D12 问题 → Vulkan 是解决方案
- ✅ rexglue Vulkan 支持完整可用
- ✅ UnleashedRecomp-NX 使用 Plume Vulkan + NVK
- ✅ D:\Nintendo 确认 Switch 原生 NVN + Vulkan 均可用
- ✅ 选定方案 A：Plume Vulkan（Windows + Switch 统一）
- [ ] Step 8.1: 启用 Plume Vulkan 后端
- [ ] Step 8.2: 添加 Vulkan 渲染路径
- [ ] Step 8.3: 完整渲染管线测试
- [ ] Step 8.4: Switch 移植准备

### Past sessions
- Phase 1-7: see git log for details
