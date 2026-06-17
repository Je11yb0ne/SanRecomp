# Findings — SanRecomp GTA V 重编译

## 2026-06-17: GPU 渲染路径全面调查

### 调查 1: GTA V 不调用 D3D Wrapper 函数
- 钩子: sub_829D3400 (CreateTexture), sub_829D3520 (CreateVertexBuffer), sub_829D4EE0 (DrawIndexedPrimitive) 等 15 个函数
- 运行 60 秒——**零次调用**
- 结论：GTA V 不使用 D3D API 抽象层（与 Sonic Unleashed 完全不同）

### 调查 2: PM4 环形缓冲区始终为空
- 扫描了 guest 地址 0x83000000（1MB）、0x40000000、0x20000000、0x00000000
- 0x83000000：完全为零
- 0x00000000：有 171 非零 dwords，但是**静态**数据（可能是 boot vectors）
- 结论：游戏不通过 PM4 命令缓冲渲染

### 调查 3: GPU MMIO 拦截 ✅
- 修改 ppc_config.h：覆盖 PPC_MM_STORE_U32/LOAD_U32 宏
- 拦截地址范围 0x7FC80000-0x7FCFFFFF
- 每帧捕获 3 个寄存器写入：
  - `reg=0x0C80 val=0x0000000C` — 可能是 DC（显示控制器）或 CP 寄存器
  - `reg=0x0F11 val=0x00000000` — 可能是显示表面配置
  - `reg=0x01C5 val=0x00000000` — CP_RB_WPTR（写指针）
- 约 8 次写入/帧（每 ~16ms），重复相同模式

### 调查 4: Vd* 内核函数调用情况
| 函数 | 调用情况 |
|------|---------|
| VdGetSystemCommandBuffer | ✅ 每帧调用，LR=0x83639A40 |
| VdSwap | ✅ 每帧调用 |
| VdRetrainEDRAM | ✅ 每帧调用 |
| VdCallGraphicsNotificationRoutines | ✅ 每帧调用 (STUB) |
| VdInitializeRingBuffer | ❌ 从未调用 |
| VdSetDisplayMode | ❌ 从未调用 |
| VdInitializeEngines | ❌ 从未调用 |

### 调查 5: UnleashedRecomp 参考
- Sonic Unleashed 使用纯 D3D 钩子模式（GUEST_FUNCTION_HOOK）
- 所有 Vd* 函数都是 STUB
- 渲染命令通过 moodycamel 队列传送到渲染线程
- GTA V 完全不同——不走这个模式

### 调查 6: rexglue-sdk 的 PM4 命令处理器
- rexglue 有完整的 PM4→D3D12 翻译（~5000 行代码）
- 支持所有 PM4 包类型（Type-0/1/2/3、DRAW_INDX、SET_CONSTANT 等）
- 对 GTA V 无用——游戏不写 PM4 命令

### 调查 7: TDURE（Test Drive Unlimited）参考
- 使用 rexglue v0.7.4 完整集成
- D3D12 only，无 Vulkan
- main.cpp 仅 12 行

### 调查 8: Xbox-360-Crypto 参考
- Python 实现的 XeCrypt 原语
- 无 XEX 解析代码
- 对当前项目无用

### 调查 9: RPF 游戏资产
- 已复制到 `%APPDATA%/SanRecomp/game/`
- common.rpf (12MB), xbox360a.rpf (679MB), xbox360b.rpf (1.3GB), audio_rel.rpf (3MB)
- VFS 索引：6 文件、3 目录、2.1GB
- 资产可用但游戏在 init 阶段卡住（忙等），还未开始加载

### 调查 10: 忙等状态
- 游戏卡在 `0x838871A4` 忙等
- PPC_LR=0（调用者为空——可能是跳转到 null 函数指针）
- 忙等破解器通过轮转值（0x10→0x00→0xFF→...）推动前进
- _xstart 最终返回（游戏主初始化完成）
- 但 GPU 初始化可能需要更多内核服务或自然等待

## 关键代码位置

| 文件 | 改动 |
|------|------|
| `SanRecompLib/ppc/ppc_config.h` | GPU MMIO 拦截宏（PPC_MM_STORE_U32/LOAD_U32 覆盖） |
| `SanRecompLib/ppc/gpu_mmio_capture.cpp` | GPU 寄存器写入捕获缓冲区 |
| `SanRecomp/gpu/video.cpp` | D3D 钩子（已启用但未触发） |
| `SanRecomp/kernel/imports.cpp` | VdSwap PM4 扫描、VBlank 定时器 |
| `SanRecomp/os/win32/media_win32.cpp` | WinRT → stub 修复 |
