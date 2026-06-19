# GAME PLAN — Autonomous Work Guide

**Goal:** Produce a working `SanRecomp.exe` that renders GTA V graphics on screen.

**Rule:** Never stop. Update after each phase. Just work.

**Current Phase: 13 — 加载死锁 + 长期方案规划** ⚡ NEXT

---

## Phase 11 Goal (2026-06-19)

**GTA V 渲染真实游戏画面 — 安装 8GB STFS 数据让 `game:` 解析游戏资源文件。**

### 诊断闭环（本次会话）
游戏已是**稳定交互状态**（60Hz present + 轮询 `XamInputGetState user=0..3`），停在安装/加载界面。
它早已冲过"是否已安装"判断，正在**加载资源但全部 `entry not found`**：
```
game:\xbox360\textures\startup.#td        ← 找不到
game:\common\data\startup.meta            ← 找不到
game:\xbox360\anim\creaturemetadata.rpf   ← 找不到
```
`game:` = `D:\Games\Xenia\gta5`，只有顶层 `.rpf` 归档，没有 `xbox360\`/`common\` loose 树。

**8GB 安装 = 4 个 PIRS STFS 包**（已定位 + 确认 magic=`PIRS`）：
```
D:\Games\Xenia\gta5\content\0000000000000000\545408A7\00000002\545408A70000000{0,1,2,3}
（每个 ~2GB，共 ~8GB，里面是带正确文件名的 xbox360\/common\ 树）
```
**为何游戏认为没安装**：rexglue 枚举器 `XamContentCreateEnumerator→ListContent` 扫的是 userRoot(`AppData\...\save`)，
且只枚举目录、跳过原始包文件 → 0 items → 安装提示。

### 关键事实
- rexglue 自带 STFS 提取：`StfsContainerDevice` + `ContentManager::InstallContent`（content_manager.cpp:572）
- 预编译 SDK 暴露了 `rex/filesystem/devices/stfs_container_device.h` + `entry.h`/`file.h` → 可写独立解包器
- OpenIV 解包（`D:\Games\Xenia\gta5 extract\`）**不可用**：文件名被改（`.#td`→`.xtd`、`.meta`→`.xml`）且不完整
- 参考：skate3 `RunRexglueIsoInstallWizardBlocking` + `ExtractAll`（skate3_iso_installer.cpp）= 安装器 UI 模板

### 步骤
1. 写独立 STFS 解包器（rexglue `StfsContainerDevice`），解包 1 包验证文件名正确
2. 解包全部 4 包 merge 进 `game:` 根（产出 `xbox360\`+`common\` 树）
3. 重跑，查 fs 日志资源解析 + 游戏是否冲过加载界面
4. 通过后按 skate3 模式包装成 in-app 安装器 UI（自动检测/选 STFS 源 → 解包 → 进游戏）

### 待验证未知
- 游戏读安装数据走 `game:` 还是 content-mount root？→ 先直接 merge 进 `game:` 根（请求路径就是 `game:\xbox360\...`，最可能正确），实验确认

### Phase 11 进展（2026-06-19 会话）

**✅ 已完成 + 重大发现：**
1. **STFS 解包器** `test_rexglue/stfs_extract.cpp`（用 rexglue `StfsContainerDevice`）+ CMake target `stfs_extract`。已构建可用。
2. **8GB 安装已解包**：4 个 PIRS 包 → 每个含 `partN.rpf`（RPF7 归档）。解包到 userRoot 内容布局：
   `C:\Users\Jellybone\AppData\Roaming\SanRecomp\save\0000000000000000\545408A7\00000002\545408A70000000{0..3}\partN.rpf`
3. **枚举成功**：`XamContentCreateEnumerator` 0 → 4 items（content_type 2 正确，路径匹配 rexglue `ListContent`）。
4. **修复 rexglue utf8 崩溃 bug**：`rex::string::to_utf16`（src/core/string.cpp:34）用检查版 `utf8::utf8to16`，遇非法 UTF-8 抛 `utf8::invalid_utf8` → 崩溃。改为先 `replace_invalid` 再转换（非抛出，同 Xenia 容错）。**已改 + 重建 DLL**。
   - 诊断手段：main.cpp CrashHandler 解码 MSVC C++ 异常类型名；VEH first-chance 捕获 + `CaptureStackBackTrace`；`llvm-symbolizer + rexruntimerd.pdb` 符号化定位到 `ResolvePackagePath→to_utf16`。

**🔴 当前精确阻塞：游戏用未初始化 content_data 打开安装内容 → 脏盘错误**
- 修 utf8 后不再崩溃，但游戏报 **`XamShowDirtyDiscErrorUI`**（脏盘/读取错误）。
- 给 rexglue `xeXamContentCreate` 加诊断日志，确认游戏行为：
  - 游戏调 `XamContentCreateEx`（客户机 `sub_8363A408` @ recomp.182.cpp:37222），打开 **5 个 root：part0/part1/part2/part3/common**，flags=3（OPEN_EXISTING）。
  - **content_data 完全是未初始化垃圾**：device_id/content_type/display_name/file_name 全是宿主指针（exe 0x7FF7… + dll 0x7FFA… 混合），content_type 每次运行都变（ASLR）。**只有 root_name 干净**。
  - 游戏**不调 XamEnumerate**（只创建枚举器拿计数，然后按硬编码 root 打开）。
  - rexglue 用 file_name 定位包 → 垃圾 file_name → `exists=false` → 5 次全失败 → 脏盘。

**🔧 下一步（精确）：**
- content_data 由 `sub_8363A408` 的**调用者**构建。需向上追这个客户机调用链，找出 content_data 为何未初始化（可能：重编译丢了初始化 / 缺前置调用 / 游戏期望 zero-init）。
- 备选策略：让 rexglue 按 root_name 或 device_id 定位内容（hacky）；或对比 Xenia 如何处理 GTA V 这种 content open。
- **诊断已就位**：rexglue xeXamContentCreate 有 `[diag]` 日志；DLL 增量重建快（仅改动文件 + 链接）。

**构建/诊断要点：**
- rexglue DLL 增量重建：`refs/rexglue-sdk/out/build/vulkan/` → `ninja <out路径>/rexruntimerd.dll` → 复制到 `test_rexglue/out/easy/`。
- 符号化崩溃：`llvm-symbolizer --obj=rexruntimerd.dll --relative-address 0xRVA`（PDB 在 refs/rexglue-sdk/out/win-amd64/）。
- backtrace 里**巨大偏移**=其他模块（vcruntime 等），**小偏移(<0xF00000)**=rexruntimerd 真实帧。

### 🎉🎉 突破：冲过脏盘错误 + 游戏渲染（2026-06-19 续）

**修复链（脏盘→渲染）：**
1. **utf8 容错**（已重建 DLL）：`to_utf16` replace_invalid。
2. **按 root_name 挂载内容**（UnleashedRecomp 模式，已重建 DLL）：
   - 新增 `ContentManager::OpenContentByRootName(root_name)`（content_manager.cpp）：挂载 `<userRoot>/0/545408A7/00000002/<root_name>/`。
   - `xeXamContentCreate` OPEN_EXISTING 回退：按 file_name 找不到时（GTA V content_data 未初始化）→ 按 root_name 挂载。
   - 内容目录重命名 `545408A70000000{0..3}` → `part0..part3`（匹配游戏 root 名）。
3. **结果**：✅ 0 脏盘错误 ✅ 无崩溃 ✅ part0-3 内容挂载成功 ✅ **VdSwap 660+ 帧稳定渲染**。

**🔴 当前阻塞：游戏资产文件缺失（最后一道数据门槛）**
- 游戏把 content open 当**存在性检查**（开→立即关，不读 partN.rpf）。
- 真实资产读取走 `game:\xbox360\...` / `game:\common\...`，全部 `entry not found`：
  - 嵌套归档：`game:\xbox360\anim\creaturemetadata.rpf`、`scaleform_*.rpf`、`streamedpeds_*.rpf`...
  - 资源：`game:\xbox360\textures\*.#td`、`models\*.#dd`
- **结论**：GTA V 的"安装"= 把 4 个 partN.rpf 的**内容解包到 `game:\xbox360\` + `game:\common\`**（含嵌套 rpf + 资源）。游戏从不按名读 partN.rpf。

**🔧 下一步：提供 game:\xbox360\ + game:\common\ 资产**
- 需要 **RPF7 解包**（partN.rpf 是 RPF7 归档，AES TOC + 压缩），且**保留 RAGE 文件名**（游戏要 `.#td`，OpenIV 解包成 `.xtd` 不匹配）。
- 候选源：(a) 解包 partN.rpf（正确文件名）；(b) 用户 OpenIV 解包的 gta5 extract（.xtd，需重命名 x→#）；(c) 写 RPF7 解包器。
- 文件名 `#` vs `x` 约定是关键：游戏请求 `startup.#td`，rexglue VFS 字面匹配。

### 🎯 RPF7 格式破解 + 验证通过（2026-06-19 续）

**关键纠正（@用户）**：磁盘文件名是 `.xtd`/`.xdd`/`.xft`（OpenIV/社区对）；游戏请求用 `#` 占位符 `.#td`/`.#dd`/`.#ft`；RAGE 查找时 `#`→`x`(Xenon)。rexglue 没做这映射 → entry not found。日志确认游戏请求 74×`.#td` + 10×`.#ft` + 6×`.#dd`。

**OpenIV 解包不可用**：①浅层（957 嵌套 rpf 没递归解）②有损（`.#td`→`.xtd` 改名、`.meta`→`.xml` **改内容**）。

**360 RPF7 格式（参考 refs/RPF7-master balika011 + refs/CodeWalker-master，已实测验证）**：
- **密钥**：`test_rexglue/keys/gtav_360_rpf.key`（32B AES-256，= `D:\Games\Xenia\gta5\(360)key.dat`）
- **Header(16B)**：magic"RPF7"，entries(BE u32)，infos(BE: bit31=platform, bit28-30=filenamesShift, bit0-27=filenamesLength)，flags(=0x0FFFFFF7 加密)
- **TOC 解密**：header 后 `entries*16 + filenamesLength` 字节，**单次 AES-256-ECB**（360；PC 才是 16 轮！）
- **Entry(16B)**：offset(3B,×512扇区,bit7=isResource)，compressedSize(3B)，nameOffset(2B BE)，[dir: subIndex(4B)+subCount(4B)] / [file: uncompressedSize(4B)+isEncrypted(4B)]
- **目录标志**：offset==0x7FFFFF；**文件名**：names blob 在 `entries*16` 偏移，名字在 `nameOffset<<shift`
- **压缩**：360 = **LZX**（rexglue 有 mspackrd / 可移植 balika011 lzx.c）；文件 offset<<9；compressedSize==0=未压缩
- **实测验证**（`test_rexglue/rpf_read.exe` 工具）：
  - xbox360a.rpf → `data/{cdimages/{scaleform_*.rpf}, lang/{american.rpf,...}}`
  - xbox360b.rpf → `textures/{analogueoverlay.xtd, frontend.xtd, ...}` ← 游戏要的纹理！
  - common.rpf → `data/{action_table.meta, common.meta, ...}` ← 真 .meta（证实 OpenIV 有损）

**方案确定：写 rexglue `RpfContainerDevice`（仿 StfsContainerDevice），挂载 xbox360a/b.rpf→game:\xbox360\、common.rpf→game:\common\**。设备内部：递归嵌套 rpf + LZX 解压 + `#→x` 解析。**纯可移植 C++ → Switch 直接复用**（用户确认要做 Switch 版）。

**下一步**：写 RpfContainerDevice（RPF7 解析已验证）→ 挂载 → 重跑确认 game:\xbox360\*.#td 解析成功 → 游戏加载真实资产。

### 🟢🟢 RpfContainerDevice 已建成 + 工作！（2026-06-19 续）

**完整实现 + 验证链：**
1. **独立验证解码管线**（`test_rexglue/rpf_read.cpp` + 移植的 `rpf_lzx.c` + `rpf_aes.c`）：common.rpf 的 `data/action/branches.meta`（加密+压缩 1410B）→ AES-256 单次 + 分块 LZX → **54519B 合法 XML**（`<?xml...><CAct...`）。**用设备的确切代码路径（bundled tiny-aes AES-256 + 分块 LZX）二次验证**，输出一致。
2. **设备实现**（`refs/rexglue-sdk/src/filesystem/devices/`）：
   - `rpf_container_device.{h,cpp}`：多归档合并、`ParseToc`（单次 AES TOC）、`ReadNodeData`（非资源→解压；资源→重建 16B RSC7 头+压缩体）、`ResolvePath`（`#→x` 回退）、Entry 树（仿 StfsContainerDevice）。
   - `rpf_aes.{c,h}`：bundled tiny-aes，配置 AES256+ECB，**public 符号重命名**（`RpfAes_*`）避免与运行时 AES-128 冲突 + extern "C" 守卫。
   - `rpf_lzx.{c,h}`：bundled cabextract/balika011 LZX。
   - CMake：3 文件加入 `src/filesystem/CMakeLists.txt`（已启用 C 语言）。
3. **挂载**（`runtime.cpp::SetupVfs`）：检测 `<game_root>/xbox360{a,b}.rpf` + `(360)key.dat`（不入 git）→ `RpfContainerDevice` 挂到 `\Device\RpfXbox360` → 符号链接 `\Device\Harddisk0\Partition1\xbox360\`（**必须带尾分隔符**，否则 prefix-match 误吞 `xbox360a.rpf`）→ `\Device\RpfXbox360\`。common.rpf 由游戏自挂（`common:` → game root）。
4. **DLL 重建成功**（无符号冲突，AES 重命名生效）+ 部署。

**运行结果（重大进展）：**
- ✅ 设备挂载：`RpfContainerDevice: mounted xbox360a.rpf (17) + xbox360b.rpf (56)`。
- ✅ 符号链接修复后游戏**冲过归档打开**（fs 日志 3 行 → **2219 行**），打开 `game:\xbox360a.rpf`（host 真文件，游戏自挂 packfile）。
- ✅ 游戏继续 **60Hz 渲染**（911 render 行），无崩溃。
- ✅ VFS 解析链验证正确：`game:\xbox360\X` → game:→Partition1 → 尾分隔符符号链接 → RpfXbox360 设备。

**🔴 新发现 = 下一阶段：game:\xbox360\ 需要「多归档合并」（不止 xbox360a/b）**
- 游戏卡在请求 `game:\xbox360\{data\cdimages\scaleform_generic.rpf, anim\creaturemetadata.rpf, audio\occlusion.rpf, models\cdimages\streamedpeds_*.rpf, levels\...\cutspeds.rpf}` —— 这些**不在 xbox360a/b**（a 只有 scaleform_frontend/platform_360 + lang；b 只有 UI 纹理/模型/movies）。
- 它们在 **8GB 安装（partN，原 STFS 545408A700000000-3）**。已提取的 part0-3 目录（7.7G/1297 文件）：levels 较全（cutspeds 找到），但 **缺 data/、audio/，且 creaturemetadata/scaleform_generic/streamedpeds 未提取到**（浅层提取）。
- **完整 game:\xbox360\ = xbox360a + xbox360b + audio_rel + 4×partN 安装** 的合并。盘内 + 安装**互补**（盘内 textures=UI，安装 textures=script_txds 等内容 rpf）。

**下一步选项**：(a) VFS overlay（改 ResolvePath 试所有匹配设备）+ 挂载 RpfContainerDevice(xbox360a/b) + 4×StfsContainerDevice(原 STFS 安装包，按需读取完整内容) 叠加在 game:\xbox360\；(b) 用 stfs_extract 把安装包完整提取（非浅层）；(c) 扩展 RpfContainerDevice 同时吃 STFS/loose 源。推荐 (a)——StfsContainerDevice 按需读取，绕过提取不完整问题，VFS overlay 是通用小改动。

### 🟢🟢🟢 6 归档合并完成 — 真实资产加载！（2026-06-19 续）

**关键发现：STFS 包内含 partN.rpf（RPF7 归档），不是 loose 树。**
- `stfs_extract --list 545408A700000000` → `part0.rpf (2.0GB)` + `part0.timestamp`。即 STFS 根只有 partN.rpf 这个 RPF7 归档文件 + 时间戳。
- 之前的 STFS overlay 给的是 partN.rpf 文件本身（不是其内容），所以 `\levels\...` 解析失败。
- **正解**：把 partN.rpf RPF7 归档**提取出来**（`stfs_extract <pkg> D:\Games\Xenia\gta5\install\` → `install\part{0-3}.rpf`，~7.7GB），用 RpfContainerDevice 解析。

**实现（已完成）：**
1. **VFS overlay**（`virtual_file_system.cpp::ResolvePath`）：最长前缀匹配 + 同前缀设备**并集**（试每个直到解析成功）。保留 NullDevice 语义（更长前缀 Partition1 不被 \Device\Harddisk0 遮蔽）。通用小改动。
2. **提取 partN.rpf**：4 个 STFS 包 → `install\part0-3.rpf`（2.0+2.0+2.0+1.7GB）。part0 验证：RPF7 676 条目，根 = `levels/gta5/_citye/...`（世界数据，直接映射 game:\xbox360\levels\）。
3. **6 归档合并**（`runtime.cpp::SetupVfs`）：RpfContainerDevice 现挂载 **xbox360a + xbox360b + install\part0-3.rpf**（archive 列表合并），全部在 game:\xbox360\。移除 STFS overlay（改为直接挂 partN.rpf）。
4. DLL 重建 + 部署。

**运行结果（重大里程碑）：**
- ✅ 6 归档全挂载：xbox360a(17) + xbox360b(56) + part0(676) + part1(342) + part2(212) + part3(148) = 1451 条目。
- ✅ **entry-not-found：满屏 → 仅剩 ~13 个真实缺失文件**。之前缺的 streamedpeds_hc/ig 等现在解析成功（从 install）。
- ✅ **游戏加载真实资产**：scaleform_platform_360/frontend.rpf、levels/models、streamedpeds 子集等都从合并归档解析。
- ✅ **游戏存活 + 60Hz 渲染**（1070 VdSwap），fs 行 2219→2544，请求推进到新资产（icon.PNG、data/metadata/cameras）。
- ✅ **剩余 ~13 缺失文件是「可选探测」**：每个只请求 1 次（不重试/不死循环），游戏容忍继续。盘内+安装都没有（creaturemetadata、occlusion、scaleform_generic/web/minigames/minimap、部分 streamedpeds、icon.PNG、cameras）——可能光盘 dump 不全 / DLC / 引擎可选。

**🔴 当前状态 = 资产门槛已过，下一阶段是「引擎推进到 logo/菜单」**
- 游戏不再卡资产（真实资产加载中，可选缺失被容忍，存活渲染）。
- 但还没可见 logo（rockstar_logos.bik 在 xbox360b，设备有，游戏尚未请求）。游戏在资产枚举后 fs 转静（在做引擎 init / 等线程 / 处理已加载数据）。
- **下一步**：诊断引擎为何在资产加载后未推进到 logo 播放（非文件问题；可能 GPU 命令翻译、线程同步、或需更多 init）。剩余 13 可选文件回报递减，先查引擎推进。

---

## Phase 12 — 决定性根因：DISC1=安装器 / DISC2=游戏 + 共享「安装检测门」（2026-06-19）

### 🎯 用户揭示：GTA V 360 = 两张盘
- **disc1 (install)** xex md5 `58cef5ae` = **安装器**。运行它 = 显示「插入 disc1」UI。**我们之前所有 bring-up 工作（渲染 hook、缺失函数、死锁修复、资产管线）都跑在安装器上。**
- **disc2 (play)** xex md5 `62d61cde` = **游戏本体**。Xenia 玩法 = 装 disc1 → 跑 disc2。
- 两盘解压：`D:\Games\Xenia\gta5\disc1(install)\` + `disc2(play)\`（用户分开，无覆盖）。

### ✅ 已切到 disc2 重编译运行（codegen + build 成功）
- manifest：`test_rexglue/gta5_disc2_manifest.toml`（file_path=disc2 xex，out=`generated/disc2`，+6 个 `[entrypoint.functions]` 声明——disc2 共享 disc1 代码布局，同样 6 个 tail-call 目标）。
- `CMakeLists.txt` glob+include 从 `generated/default` 切到 `generated/disc2`。重链成功（200MB exe，[204/204] Linking 13:47）。
- **复用全部运行时件**（DLL 无关游戏）：RPF 资产设备 + VFS overlay + main.cpp 的 audio/input/kernel_init config + AttachWindow + 收集器，**原样复用**。main.cpp 未改（hook 地址 disc2 与 disc1 相同，链接通过）。

### ✅ disc2 是游戏本体（证据）
跑 35s：**无 FATAL**、`xstart ENTERED`、`sub_822D41E8 (render/VdSwap) count=900`、946 GPU PRESENT、2301 fs 行、请求 `frontend`(2)+`startup`(2) 菜单/启动资产、解析 `XamShowPartyUI`/`XamShowCommunitySessionsUI` 社交 UI、`XamContentCreateEnumerator: added 5 items`。——这些都是**游戏行为**，不是安装器。

### 🔴🔴 但 disc2 **仍显示「插入 disc1」**（@用户「怎么和之前显示的还是一模一样」）
**关键认知修正**：「插入 disc1」**不是因为跑错 exe**，而是一道**安装检测门**——disc1、disc2 **共享同一段检测代码**，两盘都撞。游戏检测不到安装就位 → 渲染「插入 disc1」提示（不是错误，是 UI 状态）。

**根因铁证（`xeXamContentCreate` [diag] 日志）**：
```
xeXamContentCreate root='part0' size=308 ctype=E9010000 flags=3 exists=false
  dev+ct=C0697BF0E9010000 dname=DA9FF67B... fn42=D0BB27A1...   ← content_data 全是未初始化宿主指针
```
- 游戏打开安装内容（root='part0'..'part3'，flags=3 OPEN_EXISTING）时传的 **content_data 是未初始化垃圾**（device_id/content_type/file_name/display_name = 宿主指针；ctype 每次 ASLR 变）。**只有 root_name 'part0' 干净**。
- → ContentExists 失败（exists=false）→ 游戏认为「没装」→「插入 disc1」。
- 这与 Phase 11 disc1 观察的 content_data 垃圾**完全一致** → 证明是共享门，与盘无关。

### 🔧 下一步（精确）：修安装检测门，让游戏检测到安装
1. **首选**：查 rexglue `XamContentCreateEnumerator`/`ListContent` 给游戏返回的每个内容项的 content_data（device_id / content_type=2 / file_name / display_name）是否正确填充——游戏很可能从枚举器拿 content_data 再去 open，枚举器填垃圾 → open 垃圾。
   - 文件：`refs/rexglue-sdk/src/kernel/xam/xam_content*.cpp`、`src/system/xam/content_manager.cpp`（`ListContent`、`OpenContentByRootName`）。
2. **对比**：skate3recomp 的 `IsInstalledMarketplaceContent`（`content_root/0000000000000000/Hex8(titleId)/00000002/` + `Headers/00000002/`）——确认我们是否缺 `Headers/` 安装标记。
3. **对比 Xenia**：Xenia 能玩，正是因为枚举返回正确 content_data + 完整 GoD/Headers 布局。可考虑精确复刻 Xenia content 布局。
4. 现有 `OpenContentByRootName` 回退能挂载内容（Phase 11 冲过脏盘），但**游戏更高层的安装检测仍判定未装** → 说明仅挂载不够，需要枚举/content_data 这层正确。

**诊断已就位**：`xeXamContentCreate` 有 `[diag]` 日志；DLL 增量重建快。disc2 exe 已就绪（`test_rexglue/out/easy/`）。

---

### Phase 12c — 深入修改 + Hook 绕过（2026-06-19 续）

**发现与修改（DLL + EXE）：**

1. **Content 枚举修复**：
   - 写 `.header` 文件（`Headers/00000002/{part0-3,common}.header`，含 `XCONTENT_AGGREGATE_DATA` + `license_mask=1`）
   - 创建 `common` 目录（`OpenContentByRootName` 之前找不到）
   - 实现 **ODD 枚举**（原来完全是 TODO，游戏用此检测光盘是否插入）→ 注入 1 个 disc item（device_id=ODD, file_name="common", display_name="GTA V"）

2. **Kernel API 修复**：
   - `license_mask` CVAR 默认 0→1（游戏可能需要此检测许可证）
   - `XamContentIsGameInstalledToHDD`：从 `REX_EXPORT_STUB`（不设 r3，垃圾值）→ `REX_EXPORT_STUB_RETURN(0)`（明确返回成功）
   - `XamSwapDisc`：返回 SUCCESS（失败会导致无限重试循环）

3. **游戏 Hook**（`test_rexglue/main.cpp`）：
   - `sub_8364D6C8`（XamSwapDisc 包装器）→ 强制返回 0（跳过换盘检查+内部 trap）
   - `sub_8299BD70`（光盘状态机）→ 强制返回 1（非零=光盘已插入，调用者据此标记全局状态）

4. **当前结果**：
   - Content 枚举 7 items (6 HDD + 1 ODD)，所有 root 打开成功 ✓
   - 0 次 `XamSwapDisc` 调用，0 次 PPC trap hit ✓
   - 无 FATAL/崩溃 ✓
   - **画面仍是「插入 disc1」** ❌
   - VdSwap ~14-28/30-45s（低），GPU PRESENT 正常
   - 主线程等待 sem `0xF8000A94`，偶尔活跃

5. **根因分析**：
   - 游戏代码中 Content 打开分为两类：(a) 用未初始化垃圾 content_data（fallback 到 `OpenContentByRootName` 成功），(b) 用枚举器返回的正确 data（`ContentExists` 返回 `true`，直接 `OpenContent`）。两类都成功。
   - `sub_8299BD70(state, disc)` 是顶层状态机：`state=1` → 换盘/安装路径；`state=2` → 其他路径。调用者检查返回值写全局"光盘已插入"标记。
   - Hook 已绕过这些检查，但游戏**更高层**逻辑仍决定渲染"插入 disc1" UI——可能这是一个**默认/加载状态**，或者有其他未发现的判断路径。

6. **下一步选项**：
   - 追踪 `sub_8299BD70` 以上调用链（`sub_829Axxx` 等高阶函数）
   - 正确加载 XEX 到 IDA（需要 XEX loader 插件）
   - 搜索 RPF 归档中"insert disc"UI 资源名称，反向追踪触发条件
   - 考虑"插入 disc1"可能就是加载画面，需更多时间/特定配置文件触发过渡

---

---

## Phase 13 — 加载死锁 + reblue 研究（2026-06-19 续）

### 🔍 reblue 多盘实现原理（深入代码分析）

**refs/reblue-main/** — Blue Dragon (3盘) rexglue 重编译项目。

**核心文件**：
- `src/bdengine/platform/file_dialogue.cpp` — Windows `IFileOpenDialog` 封装，弹出系统文件选择对话框
- `config/functions.toml` — 光盘相关函数命名（非 hook 实现）

**关键函数列表**（地址范围 0x826BCEC8-0x826BD5F8）：
| 函数 | 地址 | 大小 | 作用 |
|------|------|------|------|
| `rex_ValidateDiscNumber` | 0x826BCEC8 | 0x78 | 验证光盘号 |
| `rex_ExtractDiscContentSignature` | 0x826BCF40 | 0x74 | 提取内容签名 |
| `rex_SignalDiscSwapInProgress` | 0x826BCFB8 | 0x10C | 标记换盘进行中 |
| `rex_ClearDiscSwapInProgress` | 0x826BD110 | 0xA0 | 清除换盘标记 |
| `rex_WaitForDiscSwapUI` | 0x826BD438 | 0x94 | 等待 UI 完成 |
| `rex_BeginDiscSwap` | 0x826BD5F8 | 0x78 | 开始换盘 |
| `rex_j_XamShowDeviceSelectorUI` | 0x8248D710 | 0x4 | **弹出设备选择 UI** |
| `bdDirtyDiscFatal` | 0x8248D740 | 0x1C | 脏盘错误 stub |

**reblue 换盘流程（推测）**：
1. 游戏需要换盘 → `rex_BeginDiscSwap`
2. `rex_SignalDiscSwapInProgress` → 标记开始
3. `rex_j_XamShowDeviceSelectorUI` → 弹出文件对话框（`OpenFileDialogue`）
4. 用户选 ISO → `rex_ValidateDiscNumber` → `rex_ExtractDiscContentSignature`
5. `rex_WaitForDiscSwapUI` → 等挂载完成
6. `rex_ClearDiscSwapInProgress` → 清除标记
7. 游戏继续

**reblue 如何知道先读哪个盘**：
- Blue Dragon **游戏自己**决定调用哪个盘——游戏的换盘状态机知道当前需要 disc 1/2/3
- reblue 只是**替换**换盘函数的实现（文件对话框替代系统弹窗），不改变游戏逻辑
- **GTA V 同理**：游戏自己知道需要 disc1（安装）还是 disc2（游玩），我们不需要告诉它

### 🎯 对 GTA V 的启示

**我们当前 hook 的问题**：
- `sub_82985760 → 0`：直接返回"不需要换盘"，跳过了游戏的整个初始化和验证流程
- 这类似于 reblue 把 `rex_BeginDiscSwap` 等全部 stub 掉——游戏的状态机无法正确初始化

**正确做法（模仿 reblue）**：
- 不是返回固定值，而是**实现换盘流程**
- 当游戏调换盘函数时：弹出 UI 让用户选 disc 目录 → 挂载 VFS → 返回成功
- 这样游戏的状态机走完整流程，内部标记正确设置

### ⚡ 当前状态（2026-06-19 最新）

- ✅ "插入 disc1"→亮度→加载动画（hook `sub_82985760`=0 绕过换盘）
- ✅ 8 RPF 归档 3610 条目，`gpu_allow_invalid_fetch_constants=true`
- ✅ 9 个缺失函数已 codegen
- 🔴 加载故事模式无限转圈，所有工作线程 WAIT

### 📋 下一步

1. **实现 reblue 式换盘 UI**：`OnFinalizePaths` 弹出文件选择 → 挂载 disc1/disc2 → 启动游戏
2. **移除临时 hook**：让游戏走完整初始化流程
3. **排查加载死锁**：当 hook 移除后，观察游戏是否能自然完成加载

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

### 2026-06-18 (Phase 10 — rexglue Vulkan 渲染诊断)

**🟢 重大突破：rexglue Vulkan 窗口 + Presenter 工作！**

- ✅ **Vulkan Presenter 创建成功** (presenter=YES)
  - 关键：`runtime.set_app_context(&app_context)` 必须在 `runtime.Setup()` 之前调用
  - 这样 Runtime 内部的 `SetupPresentation` 才会被调用
- ✅ **Win32 窗口弹出** — `Window::Create + Open + SetPresenter`
- ✅ **Setup OK, XEX loaded, Launched** — 游戏完整启动链打通
- ✅ **VEH 页面错误处理** — `AddVectoredExceptionHandler` 自动提交内存页面
- ⚠️ **窗口黑屏** — 游戏运行但画面全黑
- 🚫 **教训：禁止 VirtualAlloc GPU MMIO (0x7FC80000)** — 会与 GPU 驱动冲突导致蓝屏

**🔴 黑屏根因已确诊（2026-06-18 晚）：FATAL Unresolved call，不是忙等也不是 GPU 翻译问题**

诊断方法（systematic-debugging）：
- 用 `REX_HOOK_RAW` 覆盖弱别名函数加追踪（hook 机制经 xstart 验证有效）
- 启用 rexglue 自带日志（`rex::InitLogging(debug, flush=trace)` → `gta5_kernel.log`）

诊断证据：
- `xstart ENTERED` 但**无 `xstart RETURNED`** → 游戏入口执行了但卡在 xstart 内部
- 渲染函数 `sub_822D41E8` **从未被调用** → 从未到达渲染循环（排除"GPU 命令未翻译"假设）
- 内核日志末行：`[critical] [FATAL] Unresolved call from 0x8255DC5C to 0x8255DC48`

根因：
- `sub_8255DC58` = thunk：`li r4,4; b 0x8255DC48`（尾调用跳到 0x8255DC48）
- 0x8255DC48 在 `sub_8255DC20`（C++ vtable 分发器）的 `bctr` **之后**，重编译器把 bctr 当终止符，**没生成 0x8255DC48 的代码** → 函数边界识别遗漏
- guest 主线程撞上 `REX_FATAL` → 线程死 → init 中断 → 黑屏（窗口存活因主线程在消息循环 + GPU/VSync 线程在跑）

问题规模：**仅 8 个 FATAL，4 个唯一未解析目标**：`0x8255DC48` `0x8255DC88` `0x8256BCC8` `0x83366BD0`

修复路径（已确认）：在 `gta5_manifest.toml` 加 `[functions]` 声明这 4 个地址为函数入口 → 重新 `rexglue codegen` → 重新构建：
```toml
[functions]
0x8255DC48 = {}
0x8255DC88 = {}
0x8256BCC8 = {}
0x83366BD0 = {}
```

**构建系统教训：`.ninja_deps` 被中途杀构建/截断管道损坏 → 每次全量重编 197 文件。修复：删 `.ninja_deps`+`.ninja_log` 后做一次完整不中断构建；之后增量构建正常。绝不中途杀 ninja。**

**🟢 修复已实施 + 游戏推进到完整 init（2026-06-18 深夜）**

- ✅ schema 修正：嵌套 manifest 必须用 `[entrypoint.functions]`（不是顶层 `[functions]`），`end` 用引号字符串
- ✅ 加了 5 个缺失函数（静态 4 + 运行时发现的 0x836E9A58、0x836E9A70）→ 0 个 FATAL
- ✅ **main.cpp 覆盖 `rex::runtime::ResolveIndirectFunction`** 作批量收集器：缺失函数不再 FATAL，而是记录到 `gta5_missing_funcs.txt` + 返回空 stub（链接通过，无重复符号）
- ✅ **游戏现在跑完整早期 init**：Bink 视频线程(BinkAsy1)、XamNotifyCreateListener、XamContentCreateEnumerator、**SetInterruptCallback(836B1768) 注册 GPU 中断回调**（渲染路径！）
- ⚠️ **新阻塞：游戏在 SetInterruptCallback 后挂起**（39s 无内核活动）— 疑似等 VBlank/GPU 中断触发回调（同 Phase 8 模式），或等 gameconfig.xml 加载
- ⚠️ `game:\common\data\gameconfig.xml` VFS 未找到（GTA V 数据在 .rpf 归档）

**IDA 调查结论（不可行）**：那个 106MB `.i64` 只有 1 段（0x0，原始压缩 XEX 当扁平数据）+ 1 函数——从没用 XEX loader 正确加载/分析。两个 IDA 安装都没 Xbox360 XEX loader。MCP 的 idalib（C:\software\IDAPRO 补丁版）也读不了。要用 IDA 需编译 idaxex 匹配 9.3 SDK + 正确加载 + 数小时分析，不值得。**缺失函数只能运行时逐个发现（稀疏，可控）。**

**下一步**：诊断 SetInterruptCallback 后的挂起 — rexglue GPU vsync worker 是否 DispatchInterruptCallback 触发游戏回调 836B1768？游戏在等什么 GPU 状态？

**🟢 VBlank 派发已确认 + 全线程死锁定位（2026-06-18 深夜2）**

- ✅ **rexglue 确实 60Hz 派发 VBlank** 给游戏中断回调 sub_836B1768（hook 计数：30s 内 1620+ 次 ≈54Hz，r4=0x4004CD00 正确 user_data）。vsync worker → MarkVblank → DispatchInterruptCallback → ExecuteInterrupt 链路工作。
- ⚠️ **但 init 仍不前进——根因是多线程同步死锁，不是缺 VBlank**
- 诊断方法：看门狗线程采样主线程 PPCContext（lr/r1/r3），+ kernel_state object_table dump 所有线程
- **主线程（id=4）阻塞在 `NtWaitForSingleObjectEx`**（sub_823C7A68 wait 包装，lr=0x823C7AA0），等**信号量句柄 0xF80008F0**（object type=8=Semaphore），22s 完全静止
- **全部 18 线程都在等**：
  - id=4~F（12线程）：NtWaitForSingleObjectEx 等各自信号量
  - id=11/12：KeWaitForSingleObject 等调度对象指针（在 GPU 代码 0x836BExxx）
  - id=10：**RtlEnterCriticalSection** 等被占的临界区锁（0x820108EC）
  - id=1/2/3：host 线程（GPU Commands/VSync/Kernel Dispatch）
- GPU 中断处理器 836B1768 只做自旋锁，不直接释放这些信号量
- **疑似根因**：(a) 游戏 init 工作线程池死锁——没有线程释放信号量启动工作；或 (b) gameconfig.xml 缺失导致加载线程错误/挂起不发完成信号；或 (c) rexglue 信号量/临界区唤醒有问题

**下一阶段**：解开这个多线程死锁 — 追踪信号量释放路径（谁该 release 0xF80008F0），对比可用 rexglue 项目（TDURE）的线程行为，或检查 gameconfig.xml 加载路径。诊断工具已在 main.cpp（看门狗 + objdump）。

**🔴 死锁深挖（2026-06-18 深夜3）：游戏 29 次 WAIT，0 次 RELEASE**

- ✅ gameconfig.xml 已提供（从 `gta5 extract/common.rpf/data/` 合并进 `gta5/common/data/`，VFS 不再 miss）——**但死锁依旧**，证明 gameconfig 不是根因（红鲱鱼；不过提取的资产过了死锁后仍需要）
- ✅ rexglue `NtWaitForSingleObjectEx` 实现正确（LookupObject→object->Wait，非 stub），wait 机制无 bug
- ✅ 排除全局锁死锁（线程分散在不同 wait 函数，全局锁在等待时正常释放）
- ✅ **hook 游戏 wait 包装 sub_823C7A68 + 释放包装 sub_8239CBA8 追踪信号量操作**
- 🔴 **关键发现：16s 内 29 次 WAIT，0 次 RELEASE** — 游戏从不释放任何信号量
  - 主线程 tid=4 等 0xF80008F0（一次，永久阻塞）
  - tid=6/7 **循环重试**等 0xF8000068（带超时，超时→重等，有执行时间但条件永不满足）
- **根因结论**：生产者/释放侧从未运行 → 所有消费者线程阻塞。最可能是 GPU/渲染线程（tid=11/12，阻塞在 KeWaitForSingleObject 等调度对象 0x4004FB4C/0x4004FBB8，在 GPU 代码 0x836BExxx）是该释放信号量的生产者，但它在等某个永不就绪的 GPU 状态/事件；GPU 中断处理器 836B1768 只做自旋锁，不 signal 它

**下一阶段策略选项**：(a) 追 tid=11/12 在 GPU 代码里等的调度对象，连接到 GPU 中断该 signal 的路径；(b) 对比 UnleashedRecomp（refs/，完整工作的全游戏 recomp）的线程/GPU 同步实现；(c) 追 tid=6/7 循环里检查的条件

**🔵 UnleashedRecomp 对比 + GPU 线程分析（2026-06-19）**

UnleashedRecomp（Sonic Unleashed，完整工作的全游戏 recomp）的**根本架构差异**：
- **把所有 GPU 硬件函数 stub 成空操作**：VdSwap、VdInitializeRingBuffer、VdSetGraphicsInterruptCallback、VdInitializeEngines、**VdCallGraphicsNotificationRoutines**（图形通知派发）全是 `!!! STUB !!!`
- **不模拟 Xbox360 GPU 硬件**（无环形缓冲、无 VBlank 中断派发）
- 用 **272 个 `GUEST_FUNCTION_HOOK`** 在引擎层 hook 游戏函数，把渲染/present 替换成原生实现驱动帧循环
- 内核 sync（NtWaitForSingleObjectEx/KeWaitForSingleObject）是标准 host 原语实现（和 rexglue 一样）

| | UnleashedRecomp | rexglue（我们）|
|---|---|---|
| GPU 层 | 全 stub | Xenia 式硬件模拟 |
| 渲染驱动 | hook 272 引擎函数原生渲染 | 游戏自跑低层 GPU+线程 |
| 死锁风险 | 规避（引擎接管）| 高（需完整硬件模拟）|

**结论**：UnleashedRecomp 靠引擎 hook **完全绕过** GPU 硬件层和这类线程死锁。rexglue 用硬件模拟，要求游戏低层 init 完全自洽——GTA V 的生产者-消费者 bootstrap 在我们环境没启动（29 WAIT 0 RELEASE）。

GPU 渲染线程 `sub_836BEEA0`：`LEAVE_GLOBAL_LOCK → KeWaitForSingleObject(调度对象 0x4004FB4C, 超时) → ENTER_GLOBAL_LOCK`，等派发对象上的工作（同消费者模式）。

**战略岔路**：(a) 继续 RE GTA V 低层 init，找缺失的"第一推动"（深、不确定）；(b) 转向引擎 hook 路线（UnleashedRecomp 式，需大量 GTA V 引擎逆向）。下一具体实验：追 tid=6/7 循环（有执行时间）里 gating 生产的条件。

**🟢🟢 死锁打破！（2026-06-19）—— rexglue 参考项目揭示根因**

下载 4 个 rexglue 全游戏项目到 refs/（skate3recomp / TheOutFit / bo2-recompiled / TDURE）。关键发现：
- **skate3recomp（完整游戏）用标准 `REX_DEFINE_APP` + `ReXApp`，src/ 无任何 GPU/线程 hook** → 死锁不是 rexglue 限制，是我们设置不全
- **根因：我们手动 `Runtime::Setup` 只设了 `cfg.graphics`，漏了 ReXApp 默认设置的三项**：
  - `cfg.audio_factory = REX_AUDIO_BACKEND(rex::audio::sdl::SDLAudioSystem)`
  - `cfg.input_factory = REX_INPUT_BACKEND(rex::input::CreateDefaultInputSystem)`
  - `cfg.kernel_init = rex::kernel::InitializeKernel`
- 游戏的生产者线程在等这些子系统初始化，没初始化 → 全线程死锁

**修复后（main.cpp 加这三项 config，符号都在预编译 lib 里）：**
- ✅ 死锁打破——游戏冲过 SetInterruptCallback，继续唤醒线程、创建监听器
- ✅ **出现信号量 RELEASE**（之前恒为 0）
- ✅ **XamInputGetState 被调用**（游戏到达输入轮询循环！）
- ⚠️ 新崩溃：`0xC0000005 mem=0x8`（空指针 +8 解引用，发生在输入轮询之后）— 这是新阶段，比死锁前进一大步

**下一步**：诊断输入轮询后的空指针崩溃（mem=0x8）。VEH 不提交 <0x10000 的地址（真空指针，非未提交页）。需 guest 侧 instrument 定位崩溃的游戏函数。

**🎉🎉 游戏到达渲染循环！（2026-06-19）VdSwap 60Hz 被调用**

崩溃根因（接力 systematic-debugging）：
- CrashHandler 加 guest 上下文 → 崩溃在 tid=6，`XamInputGetState`（thunk sub_8363DA08）内部 null+8
- `runtime.input_system()` 非空，但输入**驱动**用 null 窗口创建（`SDLInputDriver(nullptr,0)`），靠 `AttachWindow` 后补窗口
- **我们从没调用 `AttachWindow`** → 驱动 null 窗口被解引用崩溃。ReXApp 在 SetupPresentation（rex_app.cpp:199）调用它，我们手动 setup 漏了

修复（main.cpp 窗口创建后加）：
```cpp
static_cast<rex::input::InputSystem*>(runtime.input_system())->AttachWindow(window.get());
```

**结果（本会话最大成果）：**
- ✅ 无崩溃
- ✅ **渲染函数 sub_822D41E8 被 60Hz 调用（VdSwap！）—— 游戏到达渲染循环**
- ✅ **988 次信号量 RELEASE**（之前恒 0）—— 全线程正常工作
- ✅ 主线程活跃执行游戏代码（不再卡 wait）

**完整修复链（死锁→渲染）= 补全 ReXApp 的手动等价物：**
1. `cfg.audio_factory` + `cfg.input_factory` + `cfg.kernel_init`（破死锁）
2. `input_system()->AttachWindow(window)`（破输入崩溃）

**下一步**：窗口是否显示 GTA V 画面？VdSwap 在调用 = 游戏认为在渲染。若仍黑屏，查 GPU 命令是否被 rexglue 命令处理器翻译/呈现。

### 2026-06-17 (Phase 9 — GPU 命令翻译 WIP)
- ✅ SPIR-V 着色器编译：DXC 将 22 个 HLSL → SPIR-V .h 文件
- ✅ video.cpp SPIR-V 包含解除 + CREATE_SHADER 宏修复 Windows+Vulkan
- ✅ `#if 0` 管线屏障移除，SEH 回退到最小渲染模式
- ✅ VBlank 60Hz 回调确认工作，sub_822D41E8 被调用
- ✅ VdSwap → _VideoPresent() → Video::Present() 链路完整
- ✅ plume swap chain Present 调通（但无可呈现帧 — "Swapchain starved"）
- ⚠️ 全管线在空纹理创建处崩溃（0xC0000005），SEH 回退到最小模式
- ⚠️ PPC 主线程卡在忙等：轮询地址 0x838871A4，值从 0x10→0xFFFFFFFF 保持循环
- ⚠️ PPC_LR=0x822F4CC8（BSS 代码区），忙等检测器正常触发但无法打破
- ⚠️ VBlank 回调中 interruptUserData=0，导致 sub_822D41E8 的 r31=0，函数无法正常工作
- ⚠️ 游戏未完成初始化，无法注册真实的 VdSetGraphicsInterruptCallback
- ✅ 添加 init loop breakers + sub_83639628 覆盖（返回 0 跳过 XamLoaderTerminateTitle）
- ✅ 忙等破解器值轮转：0x10→0x00→0xFF→… 推动游戏状态机前进
- ✅ **游戏初始化完成！_xstart 成功返回**（用 0x00 破解忙等后）
- ✅ SDL 事件循环保持窗口存活（main.cpp）
- ✅ VBlank 从事件循环启动（而非 guest_thread），60Hz 持续运行
- ✅ sub_822D41E8 覆盖为 no-op 防止 VBlank 线程挂起
- ✅ VBlank PCR 在 0x82001000 初始化（有效 PCR 结构）
- ✅ VdGetSystemCommandBuffer 返回有效 ring buffer (0x84000000)
- ✅ **连续帧呈现：60Hz PrepareFrameAndPresent 循环正常工作**
- ✅ PrepareFrameAndPresent()：acquire→clear→barriers→execute→present 完整周期
- ✅ 帧 #1-#5, #60, #120, #180, #240, #300+ 全部成功呈现
- ✅ VBlank 线程不间断运行，无阻塞无崩溃
- ✅ **游戏渲染回调 sub_822D41E8 成功执行并调用 VdSwap**（重大突破！）
- ✅ 原始 PPC 渲染函数运行并通过 `__imp__sub_822D41E8` 调用 VdSwap
- ✅ **稳定 60Hz VBlank 循环** — 无崩溃，连续帧呈现
- ✅ **直接渲染回调注册** — 通过 g_gpuRingBuffer 绕过损坏的初始化链
- ✅ **DEVELOPMENT_HACKS.md 已创建** — 所有变通方法的全面目录
- ✅ **rexglue-sdk 内核分析** — 缺失的服务不会被游戏调用
- ✅ **图形状态实验** — 测试了三种方法（零值、自引用、小值）
- ⚠️ 渲染回调在 VdSwap 后崩溃 — 图形状态需要特定值（指针=0，计数≠0）
- ⚠️ 环形缓冲区仍为空 — 崩溃前未写入 PM4 命令
- 🔧 下一步：对 0x83830000 处的图形状态结构进行 IDA 逆向工程

### 2026-06-17 (Session — rexglue 混合架构实验 + 回退)

- ⚠️ rexglue 混合架构（rexglue PPC 文件 + XenonRecomp 运行时）导致系统死机
- ✅ **根因分析**：rexglue PPC 文件（`rex::ppc::PPCContext`）与旧运行时不兼容
- ✅ `dumpbin` 确认 WinRT DLL 依赖来自 `media_win32.cpp`（非 rexruntime）
- ✅ `media_win32.cpp` WinRT GSMTC → stub（去 `api-ms-win-core-winrt-error` 依赖）
- ✅ `dxcompiler.dll` + `dxil.dll` 部署到 exe 目录
- ✅ 回退到 XenonRecomp 纯版本（分支 `feature/xenonrecomp-phase9`）
- ✅ exe 启动验证：无 DLL 缺失、无系统死机、XEX 加载正常
- ✅ 创建 `REXRUNTIME_FIX_AND_PROJECT_PLAN.md` — 7 阶段完整规划
- 🔧 **策略决定**：rexglue 作为参考源码（内核/Vulkan/PM4），不改运行时
- ✅ **Phase 3 完成**：内核同步/计时器服务补全
  - KePulseEvent、KeInitializeEvent（新实现）
  - Timer 内核对象类 + NtCreateTimer/NtSetTimerEx/NtCancelTimer
  - KeInitializeTimerEx/KeSetTimerEx/KeCancelTimer
  - KeWaitForMultipleObjects 修复（从 always-success → 真正的 WaitAll/WaitAny 轮询）
- ✅ **Phase 5 调查完成**：GPU 诊断钩子 + PM4 环形缓冲区扫描
  - 添加 D3D 诊断钩子（CreateTexture、DrawIndexedPrimitive 等 8 个函数）
  - 添加 VdSwap 中 PM4 环形缓冲区扫描
  - **关键发现**：GTA V 不调用 D3D wrapper 函数，也不写 PM4 命令缓冲
  - VdSwap 每帧调用（游戏认为自己渲染了），但无实际绘制
  - TDURE 参考项目调查（rexglue 0.7.4 完整集成，仅 D3D12）
  - Xbox-360-Crypto 参考调查（Python crypto，对我们无用）
- 🔧 **下一步**：调查游戏 GPU 初始化缺失了什么 → 让游戏开始画东西

### 2026-06-17 (Session 2 — rexglue 正确集成)

- ✅ 读取 rexglue-sdk wiki（Getting-Started、ReXApp、Function-Overrides）
- ✅ test_rexglue 构建成功（70/70, 197 rexglue PPC 文件 + 预编译 SDK）
- ✅ 改用 `find_package(rexglue)` + `rex::runtime` 正确链接
- ✅ GTA5App 简化（TDURE 模式）+ REX_DEFINE_APP 入口
- 🔧 构建+部署 DLL → 测试启动中

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
