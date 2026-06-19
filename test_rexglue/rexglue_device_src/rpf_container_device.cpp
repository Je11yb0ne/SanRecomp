/**
 ******************************************************************************
 * RpfContainerDevice — GTA V RAGE RPF7 (Xbox 360) archive mount for ReXGlue.
 * See rpf_container_device.h for the validated on-disk format.
 ******************************************************************************
 */

#include <rex/filesystem/devices/rpf_container_device.h>

#include <algorithm>
#include <cstring>

#include <rex/filesystem.h>
#include <rex/logging.h>

#include "rpf_aes.h"
#include "rpf_lzx.h"

namespace rex::filesystem {

namespace {

uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}
uint16_t be16(const uint8_t* p) { return uint16_t((uint16_t(p[0]) << 8) | p[1]); }
void put_be32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v);
}

// AES-256-ECB single-pass decrypt of whole 16-byte blocks; trailing <16 bytes
// are left unchanged (matches RAGE's AesDecrypt).
void aes256_ecb(uint8_t* buf, size_t len, const uint8_t key[32]) {
  struct AES_ctx ctx;
  RpfAes_init_ctx(&ctx, key);
  size_t whole = len & ~size_t(15);
  for (size_t off = 0; off < whole; off += 16) {
    RpfAes_ECB_decrypt(&ctx, buf + off);
  }
}

// Chunked LZX (360 RPF7). window=16, state shared across chunks. Returns the
// produced byte count; sets *ok=false on any LZX/format error.
int lzx_chunked(const uint8_t* in, int insize, uint8_t* out, int outsize, bool* ok) {
  *ok = true;
  LZXstate* st = LZXinit(16);
  if (!st) { *ok = false; return 0; }
  int produced = 0, offset = 0;
  while (produced < outsize) {
    if (offset + 2 > insize) { *ok = false; break; }
    int tmpout, tmpin;
    if (in[offset] == 0xFF) {
      if (offset + 5 > insize) { *ok = false; break; }
      tmpout = (in[offset + 1] << 8) | in[offset + 2];
      tmpin = (in[offset + 3] << 8) | in[offset + 4];
      offset += 5;
    } else {
      tmpout = 0x8000;
      tmpin = (in[offset] << 8) | in[offset + 1];
      if (tmpin == 0) break;
      offset += 2;
    }
    if (offset + tmpin > insize) { *ok = false; break; }
    int res = LZXdecompress(st, (unsigned char*)(in + offset), out + produced, tmpin, tmpout);
    if (res != 0) { *ok = false; break; }
    offset += tmpin;
    produced += tmpout;
  }
  LZXteardown(st);
  return produced;
}

}  // namespace

// ---------------------------------------------------------------------------
// RpfContainerEntry
// ---------------------------------------------------------------------------
RpfContainerEntry::RpfContainerEntry(Device* device, Entry* parent, const std::string_view name,
                                     uint32_t archive_index, uint32_t node_index)
    : Entry(device, parent, name), archive_index_(archive_index), node_index_(node_index) {}

X_STATUS RpfContainerEntry::Open(uint32_t desired_access, File** out_file) {
  auto* dev = static_cast<RpfContainerDevice*>(device_);
  auto data = dev->ReadNodeData(archive_index_, node_index_);
  *out_file = new RpfContainerFile(desired_access, this, std::move(data));
  return X_STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// RpfContainerFile
// ---------------------------------------------------------------------------
RpfContainerFile::RpfContainerFile(uint32_t file_access, RpfContainerEntry* entry,
                                   std::vector<uint8_t> data)
    : File(file_access, entry), data_(std::move(data)) {}

void RpfContainerFile::Destroy() { delete this; }

X_STATUS RpfContainerFile::ReadSync(std::span<uint8_t> buffer, size_t byte_offset, size_t* out) {
  if (byte_offset >= data_.size()) {
    *out = 0;
    return X_STATUS_END_OF_FILE;
  }
  size_t n = std::min(buffer.size(), data_.size() - byte_offset);
  std::memcpy(buffer.data(), data_.data() + byte_offset, n);
  *out = n;
  return X_STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// RpfContainerDevice
// ---------------------------------------------------------------------------
RpfContainerDevice::RpfContainerDevice(const std::string_view mount_path,
                                       std::vector<std::filesystem::path> archive_paths,
                                       const uint8_t aes_key[32])
    : Device(mount_path), name_("RPF7") {
  for (auto& p : archive_paths) {
    RpfArchive a;
    a.path = p;
    archives_.push_back(std::move(a));
  }
  std::memcpy(aes_key_, aes_key, 32);
}

RpfContainerDevice::~RpfContainerDevice() {
  for (auto& a : archives_) {
    if (a.file) fclose(a.file);
  }
}

bool RpfContainerDevice::Initialize() {
  auto* root = new RpfContainerEntry(this, nullptr, "", 0, 0);
  root->attributes_ = kFileAttributeDirectory;
  root_entry_ = std::unique_ptr<Entry>(root);

  for (uint32_t i = 0; i < archives_.size(); ++i) {
    auto& ar = archives_[i];
    ar.file = rex::filesystem::OpenFile(ar.path, "rb");
    if (!ar.file) {
      REXFS_ERROR("RpfContainerDevice: cannot open {}", rex::path_to_utf8(ar.path));
      return false;
    }
    if (!ParseToc(ar)) {
      REXFS_ERROR("RpfContainerDevice: bad RPF7 TOC in {}", rex::path_to_utf8(ar.path));
      return false;
    }
    MergeArchive(i);
    REXFS_INFO("RpfContainerDevice: mounted {} ({} entries)", rex::path_to_utf8(ar.path),
               ar.nodes.size());
  }
  return true;
}

bool RpfContainerDevice::ParseToc(RpfArchive& ar) {
  uint8_t hdr[16];
  rex::filesystem::Seek(ar.file, 0, SEEK_SET);
  if (fread(hdr, 1, 16, ar.file) != 16 || std::memcmp(hdr, "RPF7", 4) != 0) {
    return false;
  }
  uint32_t entry_count = be32(hdr + 4);
  uint32_t infos = be32(hdr + 8);
  uint32_t flags = be32(hdr + 12);
  uint32_t names_len = infos & 0x0FFFFFFF;
  uint32_t shift = (infos >> 28) & 7;

  size_t toc_size = size_t(entry_count) * 16 + names_len;
  std::vector<uint8_t> toc(toc_size);
  if (fread(toc.data(), 1, toc_size, ar.file) != toc_size) {
    return false;
  }
  // 'OPEN' (0x4E45504F) = plaintext; otherwise single-pass AES-256-ECB.
  if (flags != 0x4E45504F) {
    aes256_ecb(toc.data(), toc_size, aes_key_);
  }
  const uint8_t* names = toc.data() + size_t(entry_count) * 16;

  ar.nodes.resize(entry_count);
  for (uint32_t i = 0; i < entry_count; ++i) {
    const uint8_t* e = toc.data() + size_t(i) * 16;
    RpfNode& n = ar.nodes[i];
    uint32_t off_raw = (uint32_t(e[0]) << 16) | (uint32_t(e[1]) << 8) | uint32_t(e[2]);
    n.is_resource = (e[0] >> 7) & 1;
    uint32_t off = off_raw & 0x7FFFFF;
    n.compressed_size = (uint32_t(e[3]) << 16) | (uint32_t(e[4]) << 8) | uint32_t(e[5]);
    uint32_t name_off = uint32_t(be16(e + 6)) << shift;
    if (name_off < names_len) {
      n.name = reinterpret_cast<const char*>(names + name_off);
    }
    if (off == 0x7FFFFF) {
      n.is_directory = true;
      uint32_t sub_index = be32(e + 8);
      uint32_t sub_count = be32(e + 12);
      for (uint32_t c = 0; c < sub_count; ++c) {
        n.children.push_back(sub_index + c);
      }
    } else {
      n.data_offset = uint64_t(off) << 9;
      if (n.is_resource) {
        n.system_flags = be32(e + 8);
        n.graphics_flags = be32(e + 12);
      } else {
        n.uncompressed_size = be32(e + 8);
        n.is_encrypted = be32(e + 12) == 1;
      }
    }
  }
  return true;
}

size_t RpfContainerDevice::ServedSize(const RpfNode& n) {
  if (n.is_resource) return size_t(16) + n.compressed_size;
  return n.uncompressed_size;
}

void RpfContainerDevice::MergeArchive(uint32_t ai) {
  if (archives_[ai].nodes.empty()) return;
  auto* root = static_cast<RpfContainerEntry*>(root_entry_.get());
  for (uint32_t c : archives_[ai].nodes[0].children) {
    MergeNode(root, ai, c);
  }
}

void RpfContainerDevice::MergeNode(RpfContainerEntry* parent, uint32_t ai, uint32_t ni) {
  const RpfNode& n = archives_[ai].nodes[ni];
  if (n.is_directory) {
    auto* child = static_cast<RpfContainerEntry*>(parent->GetChild(n.name));
    if (!child) {
      child = new RpfContainerEntry(this, parent, n.name, ai, ni);
      child->attributes_ = kFileAttributeDirectory;
      parent->children_.emplace_back(std::unique_ptr<Entry>(child));
    }
    for (uint32_t c : n.children) {
      MergeNode(child, ai, c);
    }
  } else {
    if (parent->GetChild(n.name)) return;  // first archive wins on a name collision
    auto* f = new RpfContainerEntry(this, parent, n.name, ai, ni);
    f->attributes_ = kFileAttributeNormal | kFileAttributeReadOnly;
    f->size_ = ServedSize(n);
    parent->children_.emplace_back(std::unique_ptr<Entry>(f));
  }
}

Entry* RpfContainerDevice::ResolvePath(const std::string_view path) {
  Entry* e = root_entry_->ResolvePath(path);
  if (e) return e;
  // RAGE platform-char placeholder: the game requests ".#td"/".#dd"/".#ft";
  // the archives store ".xtd"/".xdd"/".xft" (x = Xenon). Map '#' -> 'x' on miss.
  if (path.find('#') != std::string_view::npos) {
    std::string mapped(path);
    std::replace(mapped.begin(), mapped.end(), '#', 'x');
    return root_entry_->ResolvePath(mapped);
  }
  return nullptr;
}

std::vector<uint8_t> RpfContainerDevice::ReadNodeData(uint32_t ai, uint32_t ni) {
  auto& ar = archives_[ai];
  const RpfNode& n = ar.nodes[ni];

  uint32_t read_size = n.compressed_size ? n.compressed_size : n.uncompressed_size;
  std::vector<uint8_t> raw(read_size);
  rex::filesystem::Seek(ar.file, int64_t(n.data_offset), SEEK_SET);
  if (read_size && fread(raw.data(), 1, read_size, ar.file) != read_size) {
    REXFS_ERROR("RpfContainerDevice: short read at 0x{:X}", n.data_offset);
    return {};
  }
  if (n.is_encrypted) {
    aes256_ecb(raw.data(), raw.size(), aes_key_);
  }

  if (n.is_resource) {
    // Loose resource file = reconstructed 16B RSC7 header + (compressed) body;
    // the game LZX-decompresses the body itself using the flags from the header.
    std::vector<uint8_t> out(size_t(16) + raw.size());
    uint32_t version = (((n.system_flags >> 28) & 0xF) << 4) | ((n.graphics_flags >> 28) & 0xF);
    out[0] = 'R'; out[1] = 'S'; out[2] = 'C'; out[3] = '7';
    put_be32(out.data() + 4, version);
    put_be32(out.data() + 8, n.system_flags);
    put_be32(out.data() + 12, n.graphics_flags);
    if (!raw.empty()) std::memcpy(out.data() + 16, raw.data(), raw.size());
    return out;
  }

  if (n.compressed_size == 0) {
    return raw;  // stored uncompressed
  }

  // Non-resource compressed file: chunked LZX -> raw file content.
  std::vector<uint8_t> out(size_t(n.uncompressed_size) + 0x8000);
  bool ok = false;
  int produced = lzx_chunked(raw.data(), int(raw.size()), out.data(), int(n.uncompressed_size), &ok);
  if (!ok || produced != int(n.uncompressed_size)) {
    REXFS_ERROR("RpfContainerDevice: LZX decode failed for '{}' (got {} want {})", n.name, produced,
                n.uncompressed_size);
    return {};
  }
  out.resize(n.uncompressed_size);
  return out;
}

}  // namespace rex::filesystem
