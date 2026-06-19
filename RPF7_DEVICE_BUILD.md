# RPF7 设备构建指南（RpfContainerDevice）

> **目的**：让 GTA V 找到真实资产 —— 把 RAGE RPF7 归档挂载为 rexglue 文件系统设备，
> 使 `game:\xbox360\...` / `game:\common\...` 虚拟路径解析成功。这是从"游戏渲染加载界面"
> 到"渲染真实游戏画面（R★ logo / 主菜单）"的最后一道数据门槛。
>
> **状态（2026-06-19）**：设计/格式/密钥/依赖**全部验证就绪**，设备头文件已建。剩 cpp 实现 + 挂载 + 测试。
> 续建从「实现步骤」开始即可，无缝衔接。

---

## 0. 背景：为什么需要这个设备

- 游戏冲过安装/换碟/脏盘检查后，读 `game:\xbox360\...`（RAGE 虚拟路径），全部 `entry not found`。
- 数据在 RPF7 归档里（深度嵌套）。**OpenIV 解包不可用**：①浅层（957 嵌套 rpf 没递归解）②有损（`.#td`→`.xtd` 改名、`.meta`→`.xml` 改内容）。
- 解法：rexglue 直接挂载磁盘归档（`D:\Games\Xenia\gta5\` 下的 `xbox360a.rpf`/`xbox360b.rpf`/`common.rpf`），
  按需解密/解压/`#→x`，**纯可移植 C++ → Switch 版直接复用**。
- 嵌套 rpf 当 blob 文件直接服务，游戏自己的 RAGE 引擎挂载它们 —— **本设备不需递归挂载嵌套 rpf**。

---

## 1. 已验证的 RPF7 (Xbox 360) 格式规格

> 参考实现：`refs/RPF7-master`（balika011，360 专门）、`refs/CodeWalker-master`（RpfFile.cs/GTACrypto.cs）。
> 验证工具：`test_rexglue/rpf_read.cpp`（已实测三个归档，TOC 正确解密、文件名可读）。

### 密钥
- 文件：`test_rexglue/keys/gtav_360_rpf.key`（32 字节 = AES-256），= `D:\Games\Xenia\gta5\(360)key.dat`
- 字节：`A1 E7 29 39 5D 8A D1 0B 9B 7B D0 11 D5 28 69 3D 96 E2 2B D6 A2 8A AB AE B4 A6 9A C6 F9 73 62 7F`
- **不入 git**（版权，已加 .gitignore）。设备运行时从该路径读取。

### Header（16 字节，大端）
```
[0:4]   char  magic = "RPF7"
[4:8]   u32   entries        (BE)  条目数
[8:12]  u32   infos          (BE)  bit31=platform, bit28-30=filenamesShift, bit0-27=filenamesLength
[12:16] u32   flags          (BE)  加密标志 = 0x0FFFFFF7（加密）；0x4E45504F='OPEN'(明文)
```

### TOC 解密
- TOC 区 = header 之后的 `entries*16 + filenamesLength` 字节。
- **单次 AES-256-ECB 解密**（360 特有；CodeWalker PC 是 16 轮，**360 是 1 轮**！这是之前踩的坑）。
- 逐 16 字节块 `rijndaelDecrypt`，剩余不足 16 的尾部原样 memcpy（见 balika011 `AESDecode.cpp:AesDecrypt`）。

### Entry（16 字节，每条）
```
[0:3]  offset (3B 大端) -> realByteOffset = (offset & 0x7FFFFF) << 9   (×512)
                          byte0 的 bit7 = isResource
[3:6]  compressedSize (3B 大端)
[6:8]  nameOffset (2B 大端)
[8:16] 视类型:
       目录:  subIndex(4B BE) + subCount(4B BE)   —— 子条目在 nodes[subIndex .. subIndex+subCount)
       文件:  uncompressedSize(4B BE) + isEncrypted(4B BE, ==1 则加密)
```
- **目录判定**：`offset == 0x7FFFFF`（全 F 标志）。node 0 = 根目录。
- **文件名**：names blob 在 TOC 偏移 `entries*16`，名字在 `nameOffset << filenamesShift`（xbox360a 实测 shift=0）。

### 文件数据读取
1. 读 `realByteOffset` 处 `compressedSize` 字节（若 `compressedSize==0` 则读 `uncompressedSize` 字节 = 未压缩存储）。
2. 若 `isEncrypted`：单次 AES-256-ECB 解密（同 TOC 方式）。
3. 若压缩（compressedSize!=0）：**分块 LZX 解压**到 `uncompressedSize`。

### 分块 LZX（360）—— 见 `refs/RPF7-master/RPF7Console/xbox360.cpp`
- LZX window = 16 bit → `window_size = 0x10000`。**init 一次，块间共享 LZX 状态**。
- 循环直到输出满 `uncompressedSize`：
  - 若当前字节 == `0xFF`：块头 `[0xFF][outHi][outLo][inHi][inLo]`（5B），显式 in/out 大小，offset+=5。
  - 否则：块头 `[inHi][inLo]`（2B），`outSize=0x8000`，offset+=2；inSize==0 则结束。
  - `LZXdecompress(state, in+offset, out+outputSize, inSize, outSize)`；offset+=inSize；outputSize+=outSize。

### 文件名 `#` ↔ `x` 占位符（关键！）
- 磁盘/归档内文件名用平台字符：`.xtd`/`.xdd`/`.xft`（x=Xenon 360）。OpenIV/社区看到的就是这个。
- **游戏代码请求时用 `#` 占位符**：`.#td`/`.#dd`/`.#ft`（日志确认：74×.#td + 10×.#ft + 6×.#dd）。
- RAGE 在 360 上查找时 `#→x`。**设备的 ResolvePath 必须做这个映射**：字面查找失败时，把叶子名里的 `#` 替换为 `x` 重试。

### 三个归档的内部结构（实测）
| 归档 | 大小 | 顶层 | 挂载到 |
|---|---|---|---|
| `xbox360a.rpf` | 0.66GB | `data/{cdimages/{scaleform_*.rpf}, lang/{american.rpf,...}}` | `game:\xbox360\` |
| `xbox360b.rpf` | 1.33GB | `textures/{analogueoverlay.xtd, frontend.xtd, ...}, models/, movies/` | `game:\xbox360\` |
| `common.rpf` | 0.01GB | `data/{action_table.meta, common.meta, carcols.xmt, ...}` | `game:\common\` |
> xbox360a + xbox360b **联合**挂到 `game:\xbox360\`（内部 data/textures/models/movies 等子树合并）。

---

## 2. rexglue 里可用的依赖（已确认）

| 需求 | 位置 | API |
|---|---|---|
| AES-256 ECB | `thirdparty/crypto/rijndael-alg-fst.c/.h` | `rijndaelKeySetupDec(rk, key, 256)`→返回 Nr(=14)；`rijndaelDecrypt(rk, Nr, ct[16], pt[16])` 逐块 |
| LZX 分块 | 选项A: `thirdparty/libmspack` 的 `lzxd.c`（流式回调，rexglue `src/system/lzx.cpp` 有 mspack 内存文件封装可参考）<br>选项B（推荐）: 移植 `refs/RPF7-master/lzx/lzx.c`（`LZXinit/LZXdecompress/LZXreset/LZXteardown` 直接缓冲 API，**正好匹配 RPF7 分块模型**） | 见 xbox360.cpp 分块循环 |
| Device/Entry/File 框架 | `include/rex/filesystem/device.h`, `entry.h`, `file.h` | 实现 `Initialize/ResolvePath/name/...`；Entry 子类 `friend` device 后用 `parent->children_.push_back` 建树（见 stfs/host_path） |

> ⚠️ rexglue 自带的 `lzx_decompress()`（`include/rex/system/lzx.h`）每次 init/free lzxd，**不能跨块共享状态**，不适合 RPF7 分块。必须用选项 A（手动驱动 lzxd）或选项 B（balika011 lzx.c）。

---

## 3. 文件清单

- ✅ **已建**：`refs/rexglue-sdk/include/rex/filesystem/devices/rpf_container_device.h`
  （含 `RpfNode`、`RpfContainerEntry`、`RpfContainerFile`、`RpfContainerDevice`；`RpfContainerEntry` 已 `friend RpfContainerDevice`）
- ⬜ **待建**：`refs/rexglue-sdk/src/filesystem/devices/rpf_container_device.cpp`（设备 + Entry + File 实现，可合一个 cpp）
- ⬜ **待移植（若选 B）**：`refs/rexglue-sdk/src/filesystem/devices/rpf_lzx.c/.h`（从 refs/RPF7-master/lzx 移植）
- ⬜ **CMake**：把 cpp 加入设备构建（查 `src/filesystem/CMakeLists.txt` 或 src 的 glob），确保 `rijndael-alg-fst` + mspack 链入 rexruntime
- ⬜ **挂载**：`test_rexglue/main.cpp`（或 rexglue VFS 初始化处）注册设备

---

## 4. 实现步骤（续建从这里开始）

### Step A — `Initialize()`
1. `fopen` host_path_，读 16B header。校验 magic "RPF7"。
2. be32 解析 entries / infos / flags；`filenamesLength = infos & 0x0FFFFFFF`，`filenamesShift = (infos>>28)&7`。
3. 读 `entries*16 + filenamesLength` 字节 TOC。
4. 若 flags != 0x4E45504F：`rijndaelKeySetupDec(rk, aes_key_, 256)` → 逐 16B 块 `rijndaelDecrypt` 单次（尾部 memcpy）。
5. `ParseToc()`。

### Step B — `ParseToc()` → 填 `nodes_`
- `names = toc + entries*16`。
- 遍历每条 entry（16B），按 §1 解析：offset(3B)/isResource(bit7)/compressedSize(3B)/nameOffset(2B BE)/后 8B。
- `name = (char*)(names + (nameOffset << filenamesShift))`（C 字符串）。
- 目录（offset==0x7FFFFF）：`is_directory=true`；children = `[subIndex, subIndex+subCount)`。
- 文件：`data_offset=(offset&0x7FFFFF)<<9`、`compressed_size`、`uncompressed_size`、`is_encrypted`、`is_resource`。

### Step C — 建 Entry 树 + `ResolvePath`
- 在 Initialize 末尾用 `nodes_` 递归建 `RpfContainerEntry` 树（根 = node 0），`parent->children_.push_back(...)`。
  - 设 `attributes_ = is_directory ? kFileAttributeDirectory : kFileAttributeNormal|kFileAttributeReadOnly`。
  - 设 `size_ = uncompressed_size`。
  - path 用 `rex::string::utf8_join_guest_paths(parent->path(), name)`。
- `ResolvePath(path)`：`root_entry_->ResolvePath(path)`（基类按 `\` 分量走）。**失败时**：取最后一个分量，若含 `#`，把 `#`→`x` 重试（实现一个带 `#→x` 回退的解析；或重写 ResolvePath 自己分量遍历，每个分量先字面后 `#→x`）。

### Step D — `ReadNodeData(node_index)` → 完整解码字节
1. `fseek(data_offset)`；读 `compressed_size`（若 0 读 `uncompressed_size`）字节。
2. `is_encrypted` → AES 单次解密。
3. `compressed_size != 0` → 分块 LZX（§1）解压到 `uncompressed_size` 缓冲。
4. 返回 `uncompressed_size` 字节。

### Step E — `RpfContainerEntry::Open` / `RpfContainerFile`
- `Open`：`data = device->ReadNodeData(node_index_)`；`*out_file = new RpfContainerFile(access, this, std::move(data))`。
- `RpfContainerFile::ReadSync(buffer, byte_offset, out)`：从 `data_` 拷贝 `min(buffer.size, data_.size-byte_offset)`。

### Step F — CMake + 链接
- 加 cpp 到设备库 glob/列表；确认 `thirdparty/crypto/rijndael-alg-fst.c` 编进 rexruntime；mspack 已在。

### Step G — 挂载（VFS 子路径，需调研）
- `game:` 当前是 `D:\Games\Xenia\gta5` 的 HostPathDevice。要让 `game:\xbox360\` / `game:\common\` 来自 RPF 设备。
- 调研 `include/rex/filesystem/vfs.h`（`VirtualFileSystem::RegisterDevice`/`ResolvePath`/symlink 优先级）。
- **首个最小测试**：先把 xbox360b.rpf 挂到一个新根（如 `x360b:`），在 main.cpp 里 `ReadNodeData` 验证能读出 `frontend.xtd`（确认 AES+LZX 正确）。
- 然后解决 `game:\xbox360` 叠加：可能在 `test_rexglue/main.cpp` Setup 后，对 game: 的设备做覆盖，或注册符号链接 `game:\xbox360` → rpf 设备根。需看 rexglue VFS 是否支持子路径设备/多设备叠加。

### Step H — 构建 + 测试
- 重建 DLL：`refs/rexglue-sdk/out/build/vulkan/` → `ninja <绝对路径>/rexruntimerd.dll` → 复制到 `test_rexglue/out/easy/`。
- 跑游戏 ~25s，查 `gta5_kernel.log`：`game:\xbox360\textures\*.#td` 不再 `entry not found`；游戏加载真实资产、推进过加载界面。
- **验证标准**：`game:\xbox360\textures\startup.#td`（或 frontend.#td）解析成功 + 出现真实画面。

---

## 5. 测试/参考资源

- `test_rexglue/rpf_read.exe`（源 `rpf_read.cpp`）：已验证的 RPF7 TOC 读取器，可用 `--list <rpf> <key>` 调试格式。
- `refs/RPF7-master/`：balika011 360 RPF7（AES/lzx/解析全套，C++）。
- `refs/CodeWalker-master/CodeWalker.Core/GameFiles/RpfFile.cs` + `Utils/GTACrypto.cs`：PC 权威参考（注意 PC=16 轮，360=1 轮）。
- `refs/libertyv-master/`：另一 RPF 工具参考。
- 归档：`D:\Games\Xenia\gta5\{xbox360a.rpf, xbox360b.rpf, common.rpf}`。
- 安装包（已解出 partN.rpf，本路线不再需要）：`C:\Users\Jellybone\AppData\Roaming\SanRecomp\save\0000000000000000\545408A7\00000002\part{0-3}\partN.rpf`。

## 6. 已修复的 rexglue 改动（在 refs/rexglue-sdk 子模块，已重建 DLL 部署）
- `src/core/string.cpp` `to_utf16`：检查版 → `replace_invalid`（非抛出，修 utf8 崩溃）。
- `include/.../content_manager.h` + `src/system/xam/content_manager.cpp`：新增 `OpenContentByRootName`。
- `src/kernel/xam/xam_content.cpp` `xeXamContentCreate`：OPEN_EXISTING 按 root_name 挂载回退 + `[diag]` 日志。
- 内容目录重命名 `545408A70000000{0..3}` → `part0..part3`。
