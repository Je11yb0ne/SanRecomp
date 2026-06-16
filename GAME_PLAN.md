# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that boots the game.

**Rule:** After each phase, update this file. Never deviate from the current phase. Never ask "continue?" — just work.

**Current Phase: 7 — Kernel 启动链补全 → Windows 游戏启动** ⚡ EXECUTING

---

## 已完成阶段

### Phase 1-4: 构建 + 渲染 + UI + 基础启动 ✅
- SanRecomp.exe 编译/链接/运行
- D3D12 紫色窗口（最小模式）
- XEX 加载 + PPC 入口执行

### Phase 5: PPC 启动诊断 ✅
- 根因：XenonRecomp 零初始化 PPC 内存 → 未初始化数据结构 → 循环/崩溃
- 决策：主用 rexglue，XenonRecomp 辅助

### Phase 6: 内核补丁（9 个修复） ✅
- 空页哨兵、忙等检测、间接调用守卫、内存分配器、RTL 保护

---

## Phase 7: 内核启动链补全 → Windows 游戏启动

**主线：** 让 PPC 代码完成启动初始化，到达游戏主循环。
**方案：** 从 rexglue 移植缺失的内核函数到 imports.cpp，而不是完全替换 PPC 代码。
**原因：** 完全替换 200+ 文件风险太大、工期太长。移植单个内核函数可以逐步验证。

### Step 7.1: 审计缺失的内核导入

**目标：** 找出 rexglue 有但我们没有的 210 个导入中，哪些是启动路径必需的。

```
验证：列出缺失的导入函数清单，标记启动路径优先級
```

#### 7.1.1 从 rexglue 导出表提取完整导入列表
- 读取 `refs/rexglue-sdk/src/kernel/xboxkrnl/export_table.inc`
- 读取 `refs/rexglue-sdk/src/kernel/xam/export_table.inc`
- 对比我们的 `imports.cpp` 中的 GUEST_FUNCTION_HOOK

#### 7.1.2 从启动追踪识别需要的函数
- 分析上次运行的输出：KeGetCurrentProcessType, XexCheckExecutablePrivilege, XGetAVPack, ExGetXConfigSetting, RtlEnter/LeaveCriticalSection, KeBugCheckEx
- 在 rexglue 生成的 PPC 代码中搜索这些调用点
- 找出调用它们的函数还需要哪些其他导入

#### 7.1.3 优先级排序
- P0（阻塞启动）：线程创建、同步原语、内存管理
- P1（启动校验）：XAM 初始化、配置读取
- P2（后续）：音频、网络、存储

### Step 7.2: 实现 P0 内核函数（线程 + 同步）

**目标：** 让游戏的线程创建和同步机制正常工作，不再轮询死标志。

```
验证：不再出现 BUSY-WAIT 或 KeBugCheckEx，PPC 代码持续推进
```

#### 7.2.1 线程函数补全
- `KeInitializeThread` — 初始化线程对象结构
- `KeResumeThread` / `NtResumeThread` — 真正恢复线程
- `NtCreateThread` / `KeCreateThread` — 如果游戏直接调用
- 研究 UnleashedRecomp 的线程实现作为参考

#### 7.2.2 同步原语补全
- `KeInitializeEvent` — 初始化事件对象
- `KeSetEvent` — 真正设置事件（当前是 stub）
- `KeWaitForSingleObject` — 真正等待（当前是 stub）
- `KeResetEvent` — 重置事件

#### 7.2.3 定时器/延迟
- `KeDelayExecutionThread` — 线程睡眠
- `KeSetTimerEx` / `KeCancelTimer` — 定时器

### Step 7.3: 实现 P1 内核函数（XAM + 系统）

**目标：** 让游戏的 XAM 初始化完成，不再调用 KeBugCheckEx。

```
验证：KeBugCheckEx 不再出现，游戏创建主窗口/UI
```

#### 7.3.1 XAM 初始化
- `XamTaskSchedule` — 任务调度（当前 stub）
- `XamTaskShouldExit` — 任务退出检查
- `XamNotifyCreateListener` — 通知监听器
- 研究 UnleashedRecomp 和 Xenia 的 XAM 实现

#### 7.3.2 系统信息
- `XamGetSystemVersion` — 系统版本
- `XGetGameRegion` — 已实现，需验证返回值
- `XamUserGetSigninInfo` — 用户登录信息
- `XamUserCheckPrivilege` — 用户权限

#### 7.3.3 配置/HID
- 确认 ExGetXConfigSetting 所有 setting 都有返回值
- HID 相关初始化（如果启动路径需要）

### Step 7.4: 处理剩余忙等循环

**目标：** 逐个打破或修复所有 PPC 忙等循环。

```
验证：PPC 看门狗显示 r1 持续变化，r3 持续变化
```

#### 7.4.1 识别所有忙等地址
- 运行游戏 30 秒，收集所有 BUSY-WAIT 地址
- 对每个地址找到对应的 PPC 函数

#### 7.4.2 对每个忙等循环
- 如果是因为内核 stub 不完整 → 实现该 stub
- 如果是因为数据结构未初始化 → 添加强符号覆盖
- 如果是因为等待硬件寄存器 → 写入合理的伪造值

### Step 7.5: 渲染管线完善（可选，看时机）

**目标：** 如果游戏到达了渲染阶段，让完整 D3D12 管线工作。

```
验证：游戏画面渲染到窗口（不只是紫色清除色）
```

#### 7.5.1 如果 Intel 驱动仍有问题
- 尝试 Vulkan 后端（plume 支持）
- 或者换到有独立 GPU 的机器测试

#### 7.5.2 着色器编译
- 运行 XenosRecomp 转换 .fxc → DXIL
- 让着色器缓存生效

### Step 7.6: 最终验证

**目标：** 游戏启动到主菜单/游戏中。

```
验证：
1. SanRecomp.exe 启动无崩溃
2. 窗口显示游戏画面（不是紫色清除色）
3. PPC 看门狗显示持续活动（r1 不断变化）
4. 无 KeBugCheckEx 调用
5. 无 BUSY-WAIT 循环
```

---

## Phase Progress Log

### 2026-06-16 (session 3 — 总体规划 + 执行)
- [ ] Step 7.1: 审计缺失的内核导入
- [ ] Step 7.2: 实现 P0 线程/同步函数
- [ ] Step 7.3: 实现 P1 XAM/系统函数
- [ ] Step 7.4: 处理剩余忙等循环
- [ ] Step 7.5: 渲染管线完善
- [ ] Step 7.6: 最终验证

### Past sessions
- Phase 1-6: see git log for details
