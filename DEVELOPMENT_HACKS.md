# DEVELOPMENT HACKS — 绕过/存根/待补全代码记录

> **目的：** 记录开发过程中所有跳过、存根、硬编码、变通方法和未完成功能。
> 当后续需要接相关功能或补全代码时，通过本文档快速定位。

---

## 目录

1. [Phase 1-2：构建与链接](#phase-1-2构建与链接)
2. [Phase 3-4：XEX 加载与最小渲染](#phase-3-4xex-加载与最小渲染)
3. [Phase 5-6：PPC 启动与崩溃修复](#phase-5-6ppc-启动与崩溃修复)
4. [Phase 7：启动链补全](#phase-7启动链补全)
5. [Phase 8：Vulkan + 渲染循环](#phase-8vulkan--渲染循环)
6. [Phase 9-10：GPU 命令翻译 + 内核移植](#phase-9-10gpu-命令翻译--内核移植)
7. [当前活跃的绕过 — 快速参考](#当前活跃的绕过--快速参考)

---

## Phase 1-2：构建与链接

### 链接存根 (`SanRecomp/link_stubs.cpp`)

**外部库存根（完全未实现）：**

| 存根类别 | 文件位置 | 说明 | 影响 |
|---------|---------|------|------|
| ZSTD 解压 | `link_stubs.cpp:110` | `ZSTD_isError`, `ZSTD_decompress` 等 — 返回 0/空 | 无法加载压缩资源 |
| FFmpeg (avcodec) | `link_stubs.cpp:141` | `avcodec_find_decoder`, `avcodec_alloc_context3` 等 — 返回 nullptr | XMA 音频解码不可用 |
| SDL_mixer | `link_stubs.cpp:152` | 所有 SDL_mixer 函数存根 | 无音频混音 |
| GameNetworkingSockets | `link_stubs.cpp` | `GameNetworking_Init`, `GameNetworking_Kill` 存根 | 无网络功能 |
| NFD (文件对话框) | `link_stubs.cpp:196` | `NFD_Init` 返回 null, `NFD_OpenDialogMultipleN` 返回 0 | 安装程序 UI 文件选择不可用 |
| SoundTouch | `link_stubs.cpp:211` | 音频处理存根 | 音频时间拉伸不可用 |
| DxcCreateInstance | `link_stubs.cpp` | 由 `dxcompiler.lib` 解析 | ✅ 已解决 |

**PPC 内核存根（`link_stubs.cpp:235`）：**

约 50 个 `__imp__*` Xbox 360 内核导出存根，包括：
- 网络：`XNet*`, `NetDll*`, `XSocket*`
- XAM：`Xam*`, `XShow*`, `XUI*`
- 其他：`XFile*`, `XHttp*`, `XOnline*`

每个存根都是 `GUEST_FUNCTION_STUB(__imp__XXX)` 宏，仅记录调用而不执行实际操作。

**资源数据存根：**
```cpp
// link_stubs.cpp:120
const size_t g_shaderCacheEntryCount = 0;  // 着色器缓存为空
const size_t g_*_uncompressed_size = 1;     // 20+ 个资源大小存根
```

**GTA V PPC 函数存根：**
```cpp
// link_stubs.cpp:227
GUEST_FUNCTION_STUB(__imp__sub_829D1758);  // GTA V PPC 函数（未重编译）
GUEST_FUNCTION_STUB(__imp__sub_829D8860);  // GTA V PPC 函数（未重编译）
```

### 禁用的源文件 (`SanRecomp/CMakeLists.txt`)

| 文件 | 禁用原因 | 需要什么来启用 |
|------|---------|--------------|
| `gpu/imgui/imgui_font_builder.cpp` | 需要 `msdfgen/msdfgen-config.h`（cmake 生成） | 完成 msdfgen 构建配置 |
| `patches/player_limit_patches.cpp` | 未声明的 GTA V PPC 函数地址 (`sub_826A6CC8`, `sub_826AE738`) | 在 GTA V 中找到等效函数 |
| `patches/aspect_ratio_patches.cpp` 至 `patches/video_patches.cpp` (~12 个文件) | Sonicteam (GTA IV) 引用 — 需要 GTA V 重写 | GTA V 特定补丁重写 |
| `ui/message_window_stub.cpp` | 存根 — 需要 GTA V 版本 | 实现 GTA V 消息窗口 UI |
| `ui/options_menu_stub.cpp` | 存根 — 需要 GTA V 版本 | 实现 GTA V 选项菜单 UI |

### 其他构建变通方法

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecompLib/CMakeLists.txt:100` | `PPC_MMIO_DEBUG` 编译定义 | 启用忙等检测器 |
| `SanRecompLib/ppc/ppc_config.h:6` | `#define PPC_MMIO_DEBUG` | 同上 |
| `SanRecomp/CMakeLists.txt:478` | `target_link_libraries(plume)` — 已注释禁用 | 在手动启用 plume 之前已注释 |
| `SanRecomp/CMakeLists.txt:557` | `message(WARNING "DXC not found...")` | DXC 着色器编译器未在系统范围内安装；使用 `tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc.exe` |
| 根 `CMakeLists.txt` | `CMAKE_SIZEOF_VOID_P=8 FORCE` | 在 `project()` 之后，因为 clang-cl 不会自动检测 |

---

## Phase 3-4：XEX 加载与最小渲染

### XEX 加载

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `tools/XenonRecomp/XenonUtils/xex.cpp` | BSS 间隙修复用于基本压缩 | 4 块 basic 压缩计算出的 imageSize 与安全头信息中的 imageSize 不匹配 |
| `tools/XenonRecomp/XenonUtils/recompiler.cpp` | `bdz`/`bdnz`/`bdnzf` 指令的边界检查修复 | 防止某些分支指令上的越界访问 |
| `tools/XenonRecomp/XenonUtils/xex_patcher.cpp` | `_BitScanForward64` → `__builtin_ctzll` | clang-cl 兼容性 |

### 内存

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/kernel/memory.cpp:48` | **空页哨兵：** 在 PPC 地址 0x00 处预初始化链表哨兵 | 防止内存分配器中的无限链表遍历循环。写入 `0x10`（偏移 0x00、0x10、0x14）和 `0xFFFF`（偏移 0x08） |
| `SanRecomp/kernel/memory.cpp` | **分发表补丁：** `InsertFunction` 用于 `sub_823F4D38`、`sub_83625C50`、`sub_836408A0` | 通过分发表执行间接调用拦截 — 用强覆盖替换弱符号 |
| `SanRecomp/cpu/guest_thread.cpp:312` | `VirtualAlloc(g_memory.base, 0x1000, MEM_COMMIT, PAGE_READWRITE)` | 确保在 8GB VirtualAlloc 回退到 nullptr 后，低 PPC 内存页保持已提交/可写状态 |
| `SanRecompLib/ppc/ppc_context.h:39` | `PPC_ADDR32(x)` — 所有加载/存储上的 32 位地址掩码 | 防止 64 位寄存器中的 PPC 地址溢出（当值超过 32 位范围时） |

### 渲染

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/gpu/video.cpp:2050` | `g_backend = Backend::VULKAN;` 强制 | 绕过 Intel D3D12 驱动崩溃。最初是 `#if defined(SAN_RECOMP_D3D12)` |

---

## Phase 5-6：PPC 启动与崩溃修复

### 忙等检测

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecompLib/ppc/ppc_context.h:54-97` | **忙等破解器：** 检测对同一地址的 200+ 次连续 `PPC_LOAD_U32` 读取 | 游戏使用软件自旋锁等待其他线程/硬件。破解器写入轮换值（`{0, 0xFF, 0x10, 0x01, 0x20, 0x02, 0x40, 0x04, 0x80, 0x08, 0xDEADBEEF, 0xFFFFFFFF}`）以推进状态机 |
| `SanRecompLib/ppc/ppc_config.h:6` | `PPC_BUSYWAIT_BREAK_VAL` | 轮换破解值序列中的基础值 |

**忙等地址：** `0x838871A4` — GTA V 内部同步变量（软件自旋锁）

**工作原理：** 游戏将值写入 `0x838871A4`（例如 `0x10` = 待处理），然后轮询等待它改变。破解器每 200 次读取写入一个不同的值，模拟"完成"信号。游戏状态机推进到下一个阶段。游戏在初始化过程中经过 8+ 个阶段，每个阶段都命中此自旋锁。

### 堆

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/kernel/heap.cpp:42` | **o1heap → malloc：** `AllocPhysical` 使用 `malloc()` 而非 `o1heapAllocate` | `o1heapAllocate` 在 2GB 物理堆上崩溃（EXCEPTION_ACCESS_VIOLATION）。可能是在大型 arena 上的 o1heap 内部错误。**影响：** 物理分配失去确定性分配优势。**TODO：** 为大型 arena 调试 o1heap |

### PPC 函数覆盖

| 覆盖函数 | 文件位置 | 作用 | 原因 |
|---------|---------|------|------|
| `sub_8363EF28` | `imports.cpp` | 返回 0（成功）而不是调用 `KeBugCheckEx` | 防止游戏在非关键错误时关机 |
| `sub_83641190` | `imports.cpp` | 回调分发器 — 无操作（跳过回调分发） | 在未初始化数据上中断无限回调循环。许多条目的 `fn=0xFFFFFFFF`，导致每秒调用 1000+ 次无效回调 |
| `sub_8363E870` | `imports.cpp` | PPC 内存中的实际 bump 分配器 | 在 PPC 虚拟空间中从 `0x10000000+` 分配内存。限制为 16MB。在 `0xE0000000` 处 OOM |
| `sub_823F4D70`/`sub_823F4D8C`/`sub_823F4D90` | `imports.cpp` | 破坏自递归（立即返回） | 防止 TLS 初始化期间的栈溢出 |
| `sub_83625C50` | `imports.cpp` | 绕过分表查找 | 防止无效函数指针上的 `EXECUTE_FAULT` |

---

## Phase 7：启动链补全

### 初始化循环中断器（当前已移除 — 见 Phase 10）

| 已移除的覆盖 | 原始位置 | 曾做过的 | 移除原因 |
|---------|---------|------|------|
| `sub_83626A3C` | `imports.cpp` | 无操作（跳过循环） | 移除用于测试自然初始化路径 |
| `sub_83627808` | `imports.cpp` | 无操作（跳过循环） | 同上 |
| `sub_836267B0` | `imports.cpp` | 无操作（跳过循环） | 同上 |
| `sub_83639628` | `imports.cpp` | 返回 0（跳过 `XamLoaderTerminateTitle`） | 同上 |

**权衡：** 
- **有中断器：** 游戏在 ~2-3 秒内完成初始化，但跳过关键的图形设置码 → 渲染回调崩溃
- **无中断器：** 游戏通过 8+ 个自然阶段进行，但需要 60+ 秒 — 太慢，无法实际使用
- **所需方案：** 平衡方案 — 针对慢阶段的特定中断器，保留较快的阶段自然运行

### VBlank + 渲染循环

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/cpu/guest_thread.cpp:315` | 保存主线程 PCR（r13）和栈（r1）到 `g_vblank_pcr`/`g_vblank_stack` | 为主线程上下文提供 VBlank 回调 — 但值被截断（主机指针而非 PPC 地址） |
| `SanRecomp/kernel/imports.cpp:4724` | VBlank 上下文：`ctx.r1.u64 = 0x10000000; ctx.r13.u64 = 0x82001000` | 为 VBlank 线程硬编码的栈和 PCR（不正确的截断值变通方法） |
| `SanRecomp/kernel/imports.cpp:4760` | **VBlank PCR 初始化：** 在 `0x82001000` 处以有效字段初始化 PCR（tls_ptr=0, pcr_ptr=self, stack_base=0x10000000, stack_limit=0x0FF00000, current_thread, dpc_active=0） | VBlank 回调需要有效的 PCR 才能正确运行 |
| `SanRecomp/kernel/imports.cpp:4770` | **图形状态初始化：** 在 `0x83800000`-`0x83900000`（1MB）处填充自引用指针 | 防止通过 r3 的 `sub_822D41E8` 空指针解引用。每个双字指向自身（例如 `0x83800004` 处的 `0x83800004`） |
| `SanRecomp/gpu/video.cpp:3589` | **PrepareFrameAndPresent()：** 最小清除+呈现周期（acquire→clear→barriers→execute→present） | 绕过完整的渲染命令处理器（该处理器需要未崩溃的完整管线） |
| `SanRecomp/kernel/imports.cpp:4382` | `VdSwap` **修改：** 仅记录，不调用 `_VideoPresent()`。呈现由 `PrepareFrameAndPresent()` 处理 | 当从 VBlank 线程调用时，`Video::Present()` 崩溃（0xC0000005） |
| `SanRecomp/main.cpp:498` | **事件循环：** `_xstart` 返回后，SDL 事件循环保持窗口存活。VBlank 计时器从事件循环启动 | 比最初的 guest_thread.cpp 中启动更可靠 |
| `SanRecomp/main.cpp:506` | `extern "C" void _VideoPresent() { Video::Present(); }` | `VdSwap`（PPC 函数）与 plume 呈现之间的桥接函数 |

### 渲染回调

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/kernel/imports.cpp` | `sub_822D41E8` 覆盖 — 调用 `__imp__sub_822D41E8`（原始 PPC 函数）并带有 SEH 保护 | 原始函数在 VdSwap 后崩溃（0xC0000005 — 由于图形状态未初始化导致的访问违规）。SEH 捕获崩溃，使 VBlank 线程保持存活 |
| `SanRecomp/kernel/imports.cpp:4782` | 回退回调：`interruptCallback=0x822D41E8, interruptUserData=0x83830000` | 游戏从未注册其真实的 VBlank 回调（`VdSetGraphicsInterruptCallback` 在跳过图形初始化后从未被调用） |

---

## Phase 8：Vulkan + 渲染循环

### Vulkan 后端强制

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/gpu/video.cpp:2050` | `g_backend = Backend::VULKAN;` — 强制 Vulkan 专用 | 绕过 Intel D3D12 驱动崩溃 |
| `SanRecomp/gpu/video.cpp:2057` | 设备创建 `__try/__except` 包装 — 崩溃 → 切换到 D3D12 | 非常旧的 Intel GPU 驱动程序上的驱动崩溃保护 |

### 全管线崩溃（SEH 回退）

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/gpu/video.cpp:2473` | **SEH 保护：** `pipelineLayoutBuilder.begin()` 包装在 `__try/__except` 中 | `pipelineLayoutBuilder.begin(false, true)` 在 plume Vulkan 后端中导致 0xC0000005 崩溃（可能是不兼容的初始化） |
| `SanRecomp/gpu/video.cpp:2495` | **回退路径：** 如果管线设置失败 → `g_pipelineLayout = nullptr` → `BeginCommandList()` 使用最小模式（直接到交换链） | 使程序在管线崩溃后能够启动和运行 |
| `SanRecomp/gpu/video.cpp:2505` | **大规模 SEH 包装：** 整个剩余的管线设置（纹理、着色器、ImGui）包装在 `__try/__except` 中 | 管线设置过程中的任何崩溃 → 回退到最小模式 |

**已确认的崩溃点：** 空纹理创建（`g_device->createTexture` 用于空描述符）。根本原因：plume Vulkan 纹理创建问题，带有特定标志/格式。待调查。

### SPIR-V 着色器

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/gpu/video.cpp:99-122` | SPIR-V 包含 `#ifndef _WIN32` → 无条件 | 已从 DXC 编译 SPIR-V，现在在所有平台上启用 |
| `SanRecomp/gpu/video.cpp:1611-1623` | **CREATE_SHADER 宏：** 移除了仅 Windows 的 DXIL 分支 | 原来的代码：Windows 总是使用 DXIL，非 Windows 使用 SPIRV。现在：始终根据后端进行选择 |

**着色器源文件：** `SanRecomp/gpu/shader/hlsl/*.hlsl`  
**已编译的 SPIR-V：** `SanRecomp/gpu/shader/hlsl/*.hlsl.spirv.h`（22 个文件）  
**编译器命令：** `dxc -T vs_6_0/ps_6_0 -HV 2021 -all-resources-bound -spirv -fvk-use-dx-layout -E shaderMain -Fh <out> <in> -Vn g_<name>_spirv`  
**编译器位置：** `tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc.exe`

---

## Phase 9-10：GPU 命令翻译 + 内核移植

### D3D 函数钩子状态

**活跃的钩子（不在 `#if 0` 中）：**

| 钩子 | 文件位置 ~9266 | 状态 |
|------|---------|------|
| `sub_829DFAD8` → `GpuMemAllocStub` | ✅ 活跃 | GPU 内存分配器 — 返回顺序偏移量 |
| `sub_829D3520` → `GTAIV_CreateVertexBuffer` | ✅ 活跃 | 通过 plume 创建实际 GPU 缓冲区 |
| `sub_829D3400` → `GTAIV_CreateTexture` | ✅ 活跃 | 通过 plume 创建实际 GPU 纹理 |
| `sub_829D8860` → `DrawPrimitive` | ✅ 活跃 | GTA V 绘制图元（4 个参数） |
| `sub_829D4EE0` → `DrawIndexedPrimitive` | ✅ 活跃 | GTA V 统一绘制（处理两种类型） |
| `sub_829C96D0` → `SetIndices` | ✅ 活跃 | GTA V 设置索引（设备+13580） |
| `sub_829C9070` → `SetStreamSource` | ✅ 活跃 | GTA V SetStreamSource0（设备+12020） |
| `sub_829D3728` → `SetTexture` | ✅ 活跃 | GTA V 设置纹理（每帧调用 ~20 次） |
| `sub_829C9440` → `SetVertexDeclaration` | ✅ 活跃 | GTA V 设置顶点声明（设备+10456） |
| `sub_829CD350` → `SetVertexShader` | ✅ 活跃 | GTA V 设置顶点着色器（也设置 PS） |
| `sub_826FE5C0` → `DrawPrimitiveUP` | ✅ 活跃 | GTA V DrawPrimitiveUP |

**已禁用的钩子（在 `#if 0` 中）：**

| 钩子块 | 文件位置 | 禁用原因 | 启用所需 |
|---------|---------|------|------|
| `CreateTexture`, `CreateVertexBuffer` | `video.cpp:9202-9218` | "需要参数布局调查" | 分析 GTA V 的 D3D 包装器层中实际调用约定 |
| Sonic 06 D3D 钩子 (~40 个函数) | `video.cpp:9229-9306` | Sonic 06 地址 — 在 GTA V 的地址空间中不存在 | 这些是参考代码 — 需要 GTA V 等效函数 |
| 旧版 PPC_FUNC 钩子 | `video.cpp:9048-9050` | 与 GUEST_FUNCTION_HOOK 冲突 | 已弃用 — 保留作参考 |

**关键问题：** 这些活跃的钩子从未被触发，因为渲染回调 (`sub_822D41E8`) 在到达任何 D3D 函数之前崩溃。钩子基础设施已就绪 — 只需渲染回调不崩溃即可。

### GPU 环形缓冲区

| 位置 | 变通方法 | 原因 |
|------|---------|------|
| `SanRecomp/kernel/imports.cpp:4767` | 在 `StartVBlankTimer` 中分配 1MB 环形缓冲区，地址为 `0x83000000` | `VdGetSystemCommandBuffer` 是一个存根 — 我们手动分配缓冲区 |
| `SanRecomp/kernel/imports.cpp` | `VdGetSystemCommandBuffer` 修改 — 通过 `g_ppcContext->r3` 返回缓冲区地址 | 游戏期望 r3 中有缓冲区地址 |
| `SanRecomp/kernel/imports.cpp:4736` | **环形缓冲区扫描器：** 在 VBlank 回调中读取前 8 个双字 — 全部为零 | 游戏从未写入 PM4 命令（渲染回调在写入任何内容之前崩溃） |

### rexglue-sdk 内核分析（已学习，未移植）

rexglue-sdk (`refs/rexglue-sdk/src/kernel/`) 具有完整的 Xbox 360 内核实现：
- `xboxkrnl_threading.cpp`：完整的线程管理、事件、信号量、定时器
- `xboxkrnl_ob.cpp`：对象管理（命名对象、句柄表）
- `xboxkrnl_video.cpp`：VBlank/视频函数
- `xam/xam_*`：XAM（Xbox 应用程序管理器）函数（~30 个文件）

**移植状态：** 我们的大多数内核函数已经存在（从 UnleashedRecomp 移植）：
- ✅ `NtCreateEvent` — 已实现
- ✅ `KeSetEvent` — 已实现
- ✅ `KeWaitForSingleObject` — 已实现
- ✅ `KeResetEvent` — 已实现
- ✅ `ExCreateThread` — 已实现
- ✅ `KeResumeThread` — 已实现
- ✅ `KeInitializeSemaphore` — 已实现
- ✅ `KeReleaseSemaphore` — 已实现
- ❌ `KeInitializeEvent` — 缺失（但游戏从不调用 — 通过已使用 rexglue 分析验证）
- ❌ `KePulseEvent` — 缺失（游戏从不调用）
- ❌ `KeInitializeTimerEx` — 缺失（游戏从不调用）
- ❌ `KeSetTimerEx` — 缺失（游戏从不调用）

**结论：** 游戏不依赖内核事件/定时器。它使用软件内存轮询（`0x838871A4` 上的自旋锁）进行同步。

---

## 当前活跃的绕过 — 快速参考

### 🔴 关键 — 必须解决

1. **忙等破解器** (`ppc_context.h:54`) — 写入轮换值到 `0x838871A4` 以推进游戏状态机。在游戏能够自然完成初始化之前需要。
2. **物理堆 → malloc** (`heap.cpp:42`) — `o1heapAllocate` 崩溃。使用系统 malloc。
3. **全管线崩溃** (`video.cpp:2473`) — plume Vulkan 在空纹理创建时崩溃。SEH 回退到最小模式。
4. **渲染回调崩溃** — `sub_822D41E8` 在 VdSwap 后崩溃（0xC0000005）。图形状态 `0x83830000` 未初始化。
5. **游戏未进入渲染循环** — `_xstart` 调用 `XamLoaderTerminateTitle` 并返回。主游戏循环从未执行。

### 🟡 中等 — 功能缺失

6. **VBlank PCR/上下文** (`imports.cpp:4724`) — 为 VBlank 线程使用硬编码地址（`r13=0x82001000, r1=0x10000000`）
7. **图形状态初始化** (`imports.cpp:4770`) — 1MB 的自引用指针在 `0x83800000` 处。非真实游戏状态。
8. **VdSwap 已修改** (`imports.cpp:4382`) — 不调用 `Video::Present()`。呈现由 `PrepareFrameAndPresent()` 处理。
9. **回退渲染回调** — `interruptCallback=0x822D41E8, userData=0x83830000`。真实的游戏回调从未注册。
10. **SPIR-V 着色器** (`video.cpp:99`) — 通过 DXC 手动编译。未与 cmake 构建集成。

### 🟢 轻微 — 低优先级

11. **空页哨兵** (`memory.cpp:48`) — PPC 地址 0x00 处的预初始化链表节点。
12. **分发表自动修复** (`memory.h:35`) — 自动修复损坏的函数表条目。
13. **事件循环** (`main.cpp:498`) — `_xstart` 后的 SDL 事件循环。
14. **VBlank 从主事件循环启动** — 从 `guest_thread.cpp` 中移除以提高可靠性。
15. **PPC_ADDR32 掩码** (`ppc_context.h:39`) — 对所有加载/存储进行 32 位地址截断。

### ⚪ 已禁用的文件（待重写）

16. **~12 个补丁文件** — Sonicteam/GTA IV 引用，需要 GTA V 重写。
17. **imgui_font_builder.cpp** — 需要 msdfgen 构建配置。
18. **大量 Sonic 06 D3D 钩子** — 参考代码，非 GTA V 函数。

---

## 后续步骤：按优先级排列

1. **修复全管线崩溃**：调查为什么 plume Vulkan 的 `createTexture` 对于空描述符纹理失败
2. **使渲染回调工作**：找出 `sub_822D41E8` 期望 `0x83830000` 处有什么图形状态，并模拟它
3. **平衡的初始化方法**：选择性中断器用于慢速忙等阶段，让其他初始化自然运行
4. **集成 D3D 钩子**：一旦渲染回调不崩溃，活跃的钩子应自动工作
5. **XenosRecomp 集成**：运行 XenosRecomp 将 Xbox 360 `.fxc` 着色器转换为 SPIR-V/HLSL

---

*最后更新：2026-06-17（Phase 9-10 会话）*
*会话提交次数：~25（本会话）*
