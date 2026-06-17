# Task Plan — SanRecomp GTA V 重编译

**目标：** 将 GTA V Xbox 360 静态重编译到 PC，最终渲染出真实游戏画面。

**当前阶段：** Phase 6 — 帧缓冲→Vulkan 已验证（测试图案），等待游戏渲染

**分支：** `feature/xenonrecomp-phase9`

---

## Phase 6 进度

### Step 6.1: GPU 寄存器含义 ✅
- 0x0C80 = PA_SU_DEBUG_CNTL（游戏每帧写 0x0C）
- 0x0F11 = RB_ZFAIL_SAMPLES（每帧清 0）
- 0x01C5 = CP_RB_WPTR（每帧设 0 — 环形缓冲写指针归零）
- D1GRPH_PRIMARY_SURFACE_ADDRESS (0x1844), D1GRPH_CONTROL (0x1841)

### Step 6.2: EDRAM/帧缓冲定位 ✅
- EDRAM 位于物理地址 0xE0000000（10MB）
- 显示控制器手动配置（游戏 D3D init 未完成）
- TLS+312 渲染标志已设

### Step 6.3: 帧缓冲→Vulkan swapchain ✅（首个里程碑！）
- RGB 测试图案写入 EDRAM → BGRA→RGBA 转换 → staging upload
- `copyTextureRegion` → Vulkan swapchain → `present()`
- **屏幕出现彩色条纹 —— 不再只是清屏色！**

### Step 6.4: 游戏实际渲染 🔧
- 游戏仍不渲染（D3D init 永远未完成）
- 需要触发 CreateDevice 或让游戏自然完成初始化

---

## 下一步

1. 移除测试图案，让 EDRAM 复制只在游戏实际渲染时触发
2. 用 IDA 找 GTA V 的 CreateDevice 函数（可能的 0x829D 范围地址）
3. 或让 VdSetDisplayMode 真正初始化 guest D3D 设备
