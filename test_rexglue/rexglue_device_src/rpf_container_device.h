/**
 * RpfContainerDevice — mounts one or more GTA V RAGE RPF7 archives (Xbox 360) as
 * a single rexglue filesystem device, exposing the merged contents under a mount
 * path (e.g. \Device\RpfXbox360, symlinked from game:\xbox360\).
 *
 * Xbox 360 RPF7 specifics (validated against refs/RPF7-master + CodeWalker, and
 * by test_rexglue/rpf_read on the real archives):
 *  - Header(16B): "RPF7", entries(BE u32), infos(BE: bit31 platform, bit28-30
 *    filenamesShift, bit0-27 filenamesLength), flags(encryption).
 *  - TOC = (entries*16 + filenamesLength) bytes after the header, decrypted with
 *    a SINGLE AES-256-ECB pass (360; PC uses 16). Key = GTA V 360 RPF key.
 *  - Entry(16B): offset(3B, <<9 = byte offset, bit7 of byte0 = isResource),
 *    compressedSize(3B), nameOffset(2B BE); dir: subIndex(4B)+subCount(4B),
 *    file: uncompressedSize(4B)+isEncrypted(4B), resource: sysFlags(4B)+gfxFlags(4B).
 *    Directory marker: offset == 0x7FFFFF.
 *  - Per file: read compressedSize bytes (or the uncompressed size if
 *    compressedSize==0), AES-decrypt if encrypted, chunked-LZX-decompress if
 *    compressed.
 *  - A resource served as a loose file gets a reconstructed 16B RSC7 header
 *    ("RSC7", version, sysFlags, gfxFlags; big-endian) prepended to the still
 *    compressed body — the game LZX-decompresses it itself. Non-resource files
 *    are served fully decompressed.
 *  - Names are stored with the platform char (".xtd"); the game requests with a
 *    '#' placeholder (".#td"). ResolvePath maps '#' -> 'x' on a miss.
 *
 * Pure portable C++ (no Win32) so it works on Switch as well. AES via bundled
 * rpf_aes (tiny-AES, renamed), LZX via bundled rpf_lzx (cabextract/balika011).
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <rex/filesystem/device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>

namespace rex::filesystem {

class RpfContainerDevice;

// One node (directory or file) in an archive's RPF7 table of contents.
struct RpfNode {
  std::string name;          // on-disk name (e.g. "frontend.xtd")
  bool is_directory = false;
  bool is_resource = false;
  bool is_encrypted = false;
  uint64_t data_offset = 0;  // byte offset in the archive (entry.offset << 9)
  uint32_t compressed_size = 0;
  uint32_t uncompressed_size = 0;  // file entries only
  uint32_t system_flags = 0;       // resource entries only (RSC7 reconstruction)
  uint32_t graphics_flags = 0;     // resource entries only
  std::vector<uint32_t> children;  // directory entries only: indices into nodes
};

// One backing archive file + its parsed node table.
struct RpfArchive {
  std::filesystem::path path;
  FILE* file = nullptr;
  std::vector<RpfNode> nodes;  // node 0 = root directory
};

class RpfContainerEntry : public Entry {
 public:
  RpfContainerEntry(Device* device, Entry* parent, const std::string_view name,
                    uint32_t archive_index, uint32_t node_index);
  ~RpfContainerEntry() override = default;
  X_STATUS Open(uint32_t desired_access, File** out_file) override;

 private:
  friend class RpfContainerDevice;
  uint32_t archive_index_;
  uint32_t node_index_;
};

class RpfContainerFile : public File {
 public:
  RpfContainerFile(uint32_t file_access, RpfContainerEntry* entry, std::vector<uint8_t> data);
  void Destroy() override;
  X_STATUS ReadSync(std::span<uint8_t> buffer, size_t byte_offset, size_t* out) override;
  X_STATUS WriteSync(std::span<const uint8_t>, size_t, size_t*) override {
    return X_STATUS_ACCESS_DENIED;
  }

 private:
  std::vector<uint8_t> data_;  // fully prepared file bytes (decoded / RSC7-wrapped)
};

class RpfContainerDevice : public Device {
 public:
  RpfContainerDevice(const std::string_view mount_path,
                     std::vector<std::filesystem::path> archive_paths,
                     const uint8_t aes_key[32]);
  ~RpfContainerDevice() override;

  bool Initialize() override;
  void Dump(string::StringBuffer*) override {}
  Entry* ResolvePath(const std::string_view path) override;

  const std::string& name() const override { return name_; }
  uint32_t attributes() const override { return 0; }
  uint32_t component_name_max_length() const override { return 255; }
  uint32_t total_allocation_units() const override { return 128 * 1024; }
  uint32_t available_allocation_units() const override { return 0; }
  uint32_t sectors_per_allocation_unit() const override { return 1; }
  uint32_t bytes_per_sector() const override { return 512; }

  // Reads + prepares a node's full served bytes (decode, or RSC7 + body).
  std::vector<uint8_t> ReadNodeData(uint32_t archive_index, uint32_t node_index);

 private:
  bool ParseToc(RpfArchive& archive);
  void MergeArchive(uint32_t archive_index);
  // Recursively merge one node (and its subtree) into the entry tree.
  void MergeNode(RpfContainerEntry* parent, uint32_t archive_index, uint32_t node_index);
  // Served file size for a node (resource: 16 + compressed; else uncompressed).
  static size_t ServedSize(const RpfNode& node);

  std::string name_;
  std::vector<RpfArchive> archives_;
  uint8_t aes_key_[32];
  std::unique_ptr<Entry> root_entry_;
};

}  // namespace rex::filesystem
