# Task Plan — SanRecomp GTA V 重编译

**目标：** 将 GTA V Xbox 360 静态重编译到 PC，最终渲染出真实游戏画面。

**当前阶段：** Phase 6 — GPU 帧缓冲定位与 Vulkan 显示

**分支：** `feature/xenonrecomp-phase9`
**远程：** `origin` (github.com:Je11yb0ne/SanRecomp)
**备份标签：** `backup/pre-rexglue-migration`

---

## 已完成阶段

### Phase 1-8: 构建 → PPC 启动 → Vulkan ✅
- SanRecomp.exe 编译/链接/运行
- XEX 加载 (entry=0x83639888)
- Vulkan 后端 + 60Hz 渲染循环
- VBlank → sub_822D41E8 → VdSwap → Present

### Phase 3: 内核同步/计时器 ✅
- KePulseEvent、KeInitializeEvent
- Timer 内核对象 + NtCreateTimer/NtSetTimerEx/NtCancelTimer
- KeWaitForMultipleObjects 修复（从 always-success 到真正轮询）

### Phase 5: GPU 诊断 ✅ （关键突破！）
- D3D wrapper 钩子 → 确认 GTA V 不调用
- PM4 环形缓冲区 → 确认始终为空
- **GPU MMIO 拦截** → ✅ 工作！游戏通过直接写 GPU 寄存器渲染
- 每帧模式：CP 寄存器 0x0C80=0x0C、未知 0x0F11=0x00、CP_WPTR 0x01C5=0x00

---

## Phase 6: GPU 帧缓冲定位与 Vulkan 显示 ← 当前

**现状：** GPU MMIO 拦截确认了渲染路径。游戏写 GPU 寄存器来呈现帧。
画面仍是蓝色清屏——帧缓冲内容从未被复制到 Vulkan swapchain。

**假设：** 游戏将帧渲染到 Xbox 360 EDRAM（嵌入式 DRAM），然后通过显示控制器寄存器选择要显示的前缓冲区。EDRAM 在 guest 内存中的位置需要确定。

### Step 6.1: 确定 GPU 寄存器含义
- [ ] 查 Xenia/rexglue 源码确定 0x0C80、0x0F11、0x01C5 寄存器
  - reg=0x0C80 — 可能是显示控制器寄存器（DC_*）
  - reg=0x0F11 — 可能是显示表面地址
  - reg=0x01C5 — CP_RB_WPTR（写指针）
- 参考：`refs/rexglue-sdk/include/rex/graphics/registers.h`

### Step 6.2: 定位 EDRAM/帧缓冲
- [ ] 扫描 guest 内存找非零区域（可能是渲染结果）
- [ ] EDRAM 在 Xbox 360 上位于物理地址 0xE0000000
  - guest 地址 = 0xE0000000

### Step 6.3: 帧缓冲 → Vulkan swapchain
- [ ] 将 EDRAM/帧缓冲内容复制到 Vulkan 纹理
- [ ] 通过 plume pipeline 渲染到 swapchain
- [ ] 验证屏幕显示非清屏色内容

### Step 6.4: 优化与清理
- [ ] 移除不需要的 D3D 钩子代码
- [ ] 整理诊断输出
- [ ] 提交最终版本

---

## 后续阶段

### Phase 7: 完整渲染管线
- 实现 GPU 寄存器→Vulkan 翻译
- 纹理、着色器、绘制命令

### Phase 8: 资产加载
- RPF 提取工具
- 文件系统完善

### Phase X: rexglue 集成决策
- 根据 Phase 6-7 结果决定是否迁移

---

## 错误记录

| 错误 | 原因 | 解决方案 |
|------|------|---------|
| rexglue 混合架构系统死机 | PPCContext 类型不兼容（rex vs XenonRecomp） | 回退到纯 XenonRecomp，rexglue 仅作参考 |
| D3D 钩子从不触发 | GTA V 不走 D3D wrapper，使用 GPU MMIO 直接写入 | 改用 GPU MMIO 拦截 |
| PM4 环形缓冲区始终为空 | 同上，游戏不通过 PM4 命令缓冲渲染 | 改用 GPU MMIO 拦截 |
| WinRT DLL 缺失 | media_win32.cpp C++/WinRT 代码引入 api-ms-win-core-winrt-error | 替换为 stub 返回 false |
