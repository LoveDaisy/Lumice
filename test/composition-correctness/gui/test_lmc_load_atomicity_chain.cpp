// Composition chain: what a FAILED .lmc load leaves behind.
//
// Units in the chain: file_io's binary container reader × gui_state's JSON deserializer × the
// document the caller handed in.
//
// What the collaboration produces that is observable: after a load that reports failure, the
// document on screen is still the document that was on screen before. That is not what the two
// units produced together before this suite existed — the JSON section is deserialized straight
// into the caller's GuiState (and DeserializeGuiStateJson opens with `state = GuiState{}`), while
// the texture section is read afterwards and has three ways to fail. Between those two facts sits
// a window where the loader returns false having already replaced the whole document. The caller
// (DoOpen) reads that false and skips current_file_path / dirty / run_intent / ResetFrontendState,
// so the end state is the new file's contents wearing the old file's name, with an error toast on
// top. Nothing about that looks like a failed open.
//
// The three texture failures are pinned one by one rather than as a representative sample: they
// are three separate `return false` statements, and a fix that only moves one of them out of the
// window would pass a single-case suite.
//
// The fourth failure case (a corrupt JSON section) is a PIN, not a regression probe — see its
// comment. It was already atomic before this suite; what it stops is a future change that makes
// it not be.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "gui/file_io.hpp"
#include "gui/gui_state.hpp"
#include "gui/preview_renderer.hpp"
#include "gui/xyz_half_codec.hpp"

namespace lumice::gui {
namespace {

// Byte offsets into the .lmc header. The single authority for this layout is the comment block
// above kLmcMagic in src/gui/file_io.cpp ("Header: 44 bytes, little-endian / magic[4] / version:
// uint32 / flags: uint32 / json_offset: uint64 / json_size: uint64 / tex_offset: uint64 /
// tex_size: uint64"); the constants themselves are file-static there, so a test that needs to
// corrupt one section without disturbing the others has to restate the offsets. If the layout ever
// changes, kLmcVersion is bumped in the same edit (file_io.cpp calls that bump mandatory) and
// these three constants have to follow — otherwise the corruptions below land on the wrong bytes
// and these cases quietly stop testing what they name.
constexpr size_t kVersionField = 4;
constexpr size_t kFlagsField = 8;
constexpr size_t kJsonOffsetField = 12;
constexpr size_t kJsonSizeField = 20;
constexpr size_t kTexOffsetField = 28;
constexpr size_t kTexSizeField = 36;

// The 4x4 RGB pattern that goes into the texture section. Values are arbitrary but non-uniform, so
// a successful load that returns the wrong pixels does not look like a successful load.
std::vector<unsigned char> TexturePixels() {
  std::vector<unsigned char> px(4 * 4 * 3);
  for (size_t i = 0; i < px.size(); ++i) {
    px[i] = static_cast<unsigned char>((i * 7 + 11) & 0xFF);
  }
  return px;
}

// A document carrying one crystal, reachable from the serializer.
//
// The crystal has to hang off a layer entry: the .lmc JSON has no top-level crystal array — each
// entry embeds its own crystal inline, and GuiState::crystals is the runtime ID pool the entries
// index into. A crystal with no entry pointing at it serializes to nothing at all, which was
// measured here the direct way: a first draft set only crystals[0].height and the two documents
// below came out byte-identical, so the whole suite failed its own premise assertion.
//
// sun.altitude is set alongside it so the two documents differ in a scalar as well as inside the
// layer tree — a rollback that restored the vectors but not the scalars (or the reverse) is then
// not a pass.
GuiState DocumentWith(float crystal_height, float sun_altitude) {
  GuiState s{};
  s.sun.altitude = sun_altitude;
  s.crystals.emplace_back();
  s.crystals[0].height = ShapeDist{ ShapeDistType::kUniform, crystal_height, 0.5f };
  Layer layer;
  layer.entries.emplace_back();  // crystal_id 0
  s.layers.push_back(std::move(layer));
  return s;
}

// "The document currently open in the app." current_file_path and dirty are the two fields that
// carry the defect most directly: neither is written to the .lmc JSON at all, so the ONLY thing in
// a load that can change them is `state = GuiState{}` running. If they come back changed, the
// document was overwritten — there is no other path.
GuiState SentinelDocument() {
  GuiState s = DocumentWith(/*crystal_height=*/9.5f, /*sun_altitude=*/42.0f);
  s.current_file_path = "/sentinel/current-document.lmc";
  s.dirty = true;
  return s;
}

// "The contents of the file being opened." Differs from the sentinel in fields that ARE
// serialized, so the JSON comparison in ExpectLoadFailsAndDocumentUnchanged can tell the two
// documents apart.
GuiState FileDocument() {
  return DocumentWith(/*crystal_height=*/2.5f, /*sun_altitude=*/11.0f);
}

struct TempFile {
  std::filesystem::path path;
  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path, ec);  // best-effort: a teardown failure must not fail the case
  }
};

std::filesystem::path TempPath(const char* name) {
  return std::filesystem::temp_directory_path() / name;
}

std::vector<unsigned char> ReadAllBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool WriteAllBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return out.good();
}

// Native-endian memcpy, matching how the production writer emits these fields (WriteU64 memcpy-s
// the raw uint64). A cross-endian .lmc is not a case the format supports on either side.
uint64_t ReadU64Field(const std::vector<unsigned char>& bytes, size_t offset) {
  uint64_t v = 0;
  std::memcpy(&v, bytes.data() + offset, sizeof(v));
  return v;
}

void WriteU64Field(std::vector<unsigned char>& bytes, size_t offset, uint64_t value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

uint32_t ReadU32Field(const std::vector<unsigned char>& bytes, size_t offset) {
  uint32_t v = 0;
  std::memcpy(&v, bytes.data() + offset, sizeof(v));
  return v;
}

void WriteU32Field(std::vector<unsigned char>& bytes, size_t offset, uint32_t value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

// The 4x4 linear-XYZ pattern that goes into a v>=5 texture section. Deliberately not
// representable in the 8-bit bake it replaces: values above 1.0 (the clipped range the old bake
// threw away), values far below 1/255 (the quantized-to-zero range), exact zeros (a sparse
// scene's dominant texel) and a value with no short decimal expansion — a round trip that
// survived any of those by accident would still fail the others.
std::vector<float> XyzTexels() {
  std::vector<float> px(4 * 4 * 3);
  for (size_t i = 0; i < px.size(); ++i) {
    switch (i % 4) {
      case 0:
        px[i] = 0.0f;
        break;
      case 1:
        px[i] = 65.0f + static_cast<float>(i);  // the hot spot: 65x over white
        break;
      case 2:
        px[i] = 1.0e-6f * static_cast<float>(i + 1);
        break;
      default:
        px[i] = std::sqrt(static_cast<float>(i) + 0.5f);
        break;
    }
  }
  return px;
}

XyzTextureMeta XyzMeta() {
  XyzTextureMeta m;
  m.snapshot_intensity = 0.0123456f;
  m.emitted_energy = 987654.25f;
  m.mono_anchor = 3.0517578e-5f;
  m.effective_pixels = 13007;
  return m;
}

// Produce a real, valid .lmc through the production writer rather than hand-assembling a header.
// The corruptions below then edit one field of a file that the shipping code wrote, so a change to
// the writer cannot leave these cases testing a format the loader no longer reads.
//
// The texture it embeds is the 8-bit radiance-only bake (the v4 encoding): that is the section
// whose three failure exits the cases below were written against, and the production loader still
// takes exactly that path for it. The v5 float encoding has its own writer, WriteValidXyzLmc.
bool WriteValidLmc(const std::filesystem::path& path, bool with_texture) {
  PreviewRenderer preview;  // no Init(): UpdateCpuTextureData and the getters touch no GL
  if (with_texture) {
    const std::vector<unsigned char> px = TexturePixels();
    preview.UpdateCpuTextureData(px.data(), 4, 4);
  }
  return SaveLmcFile(path, FileDocument(), preview, with_texture);
}

// Same, through the CPU mirror the production Save path fills (RefreshCpuTextureForSave →
// UpdateCpuXyzTextureData): the v>=6 linear-XYZ float16 encoding, the only one the writer
// produces.
bool WriteValidXyzLmc(const std::filesystem::path& path) {
  PreviewRenderer preview;
  const std::vector<float> px = XyzTexels();
  preview.UpdateCpuXyzTextureData(px.data(), 4, 4, XyzMeta());
  return SaveLmcFile(path, FileDocument(), preview, /*save_texture=*/true);
}

// What the v6 writer stores for XyzTexels(): the same codec call it makes, so the expectation
// below is "the file holds the writer's quantization", not a second opinion about the values.
std::vector<float> XyzTexelsAsTheWriterStoresThem() {
  const std::vector<float> px = XyzTexels();
  const float scale = ComputeXyzHalfScale(px.data(), px.size());
  std::vector<uint16_t> half(px.size());
  QuantizeXyzToHalf(px.data(), px.size(), scale, half.data());
  std::vector<float> back(px.size());
  DequantizeHalfToXyz(half.data(), half.size(), scale, back.data());
  return back;
}

// A v5 file, hand-assembled: no writer produces the float32 section any more, so the read path a
// pre-v6 document takes can only be exercised from bytes laid out here by the documented format.
// Takes a v6 file the production writer just made and swaps its texture section and header
// fields — the JSON section, its offsets and the container layout are the writer's, so the only
// thing this file says on its own is "the texture is float32". The deflate stream is a single
// stored block (no compression — the format says zlib, not "compressed"), which needs no
// compressor in this target and inflates through the same stbi call a real v5 stream does.
bool RewriteAsV5FloatFile(const std::filesystem::path& path) {
  std::vector<unsigned char> bytes = ReadAllBytes(path);
  if (bytes.size() < 44u) {
    return false;
  }
  const uint64_t tex_offset = ReadU64Field(bytes, kTexOffsetField);
  const uint32_t flags = ReadU32Field(bytes, kFlagsField);
  if ((flags & 0x8u) == 0u || tex_offset == 0u) {
    return false;  // premise: the writer made a v6 float16 file
  }
  // The v5 section: the same 32-byte header with raw_byte_count in float32 units and the last
  // field back to the reserved 0u, then the stored-block zlib stream of the floats.
  const std::vector<float> px = XyzTexels();
  const uint32_t raw_bytes = static_cast<uint32_t>(px.size() * sizeof(float));
  std::vector<unsigned char> section(bytes.begin() + static_cast<std::ptrdiff_t>(tex_offset),
                                     bytes.begin() + static_cast<std::ptrdiff_t>(tex_offset) + 32);
  WriteU32Field(section, 24, raw_bytes);
  WriteU32Field(section, 28, 0u);
  section.push_back(0x78);  // zlib header: deflate, 32K window
  section.push_back(0x01);  // fastest compression, no preset dictionary, check bits valid
  section.push_back(0x01);  // BFINAL=1, BTYPE=00 (stored)
  section.push_back(static_cast<unsigned char>(raw_bytes & 0xFFu));
  section.push_back(static_cast<unsigned char>((raw_bytes >> 8) & 0xFFu));
  section.push_back(static_cast<unsigned char>(~raw_bytes & 0xFFu));
  section.push_back(static_cast<unsigned char>((~raw_bytes >> 8) & 0xFFu));
  const auto* raw = reinterpret_cast<const unsigned char*>(px.data());
  section.insert(section.end(), raw, raw + raw_bytes);
  uint32_t a = 1;
  uint32_t b = 0;
  for (uint32_t i = 0; i < raw_bytes; ++i) {  // adler32, big-endian on the wire
    a = (a + raw[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  const uint32_t adler = (b << 16) | a;
  section.push_back(static_cast<unsigned char>(adler >> 24));
  section.push_back(static_cast<unsigned char>(adler >> 16));
  section.push_back(static_cast<unsigned char>(adler >> 8));
  section.push_back(static_cast<unsigned char>(adler));

  bytes.resize(static_cast<size_t>(tex_offset));
  bytes.insert(bytes.end(), section.begin(), section.end());
  WriteU64Field(bytes, kTexSizeField, section.size());
  WriteU32Field(bytes, kVersionField, 5u);
  WriteU32Field(bytes, kFlagsField, (flags & ~0x8u) | 0x4u);
  return WriteAllBytes(path, bytes);
}

// The shared assertion. Every failure case ends here, so "the document is untouched" is stated
// once and means the same thing in all four.
void ExpectLoadFailsAndDocumentUnchanged(const std::filesystem::path& path, const char* which) {
  SCOPED_TRACE(which);

  // Premise: the two documents must actually be distinguishable, or the JSON comparison below
  // passes for the wrong reason.
  ASSERT_NE(SerializeGuiStateJson(SentinelDocument()), SerializeGuiStateJson(FileDocument()))
      << "premise broken: the sentinel and the file serialize identically, so this case cannot "
         "observe an overwrite at all";

  GuiState current = SentinelDocument();
  LmcTexture tex;

  EXPECT_FALSE(LoadLmcFile(path, current, tex))
      << "premise broken: this file was supposed to be unloadable, so the corruption did not take";

  EXPECT_EQ(current.current_file_path, SentinelDocument().current_file_path)
      << "a failed load cleared the path of the document that is still open";
  EXPECT_TRUE(current.dirty) << "a failed load cleared the dirty flag of the document that is still open";

  // Whole-document comparison, not a handful of probes: any serialized field that moved shows up
  // here, and gtest prints both strings, so a red run names the field instead of saying "false is
  // not true". The sentinel's 9.5 against the file's 2.5 is what the diff reads as.
  EXPECT_EQ(SerializeGuiStateJson(current), SerializeGuiStateJson(SentinelDocument()))
      << "a failed load replaced the open document with the contents of the file it could not load";
}

// --- The three texture-section failures ------------------------------------------------------
//
// All three keep the JSON section intact and valid, so the loader gets all the way past
// DeserializeGuiStateJson before it fails. That is the window this task exists to close.

TEST(LmcLoadAtomicityChain, ZeroTexSizeWithFlagSetRollsBackWithoutTouchingCurrentDocument) {
  TempFile f{ TempPath("lumice_lmc_atomicity_zero_tex_size.lmc") };
  ASSERT_TRUE(WriteValidLmc(f.path, /*with_texture=*/true));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  ASSERT_GT(bytes.size(), kTexSizeField + 8u);
  ASSERT_NE(ReadU64Field(bytes, kTexSizeField), 0u) << "premise: the file was written with a texture";
  WriteU64Field(bytes, kTexSizeField, 0);  // has_texture flag left set — that is the inconsistency
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "tex_size == 0 with the has_texture flag set");
}

TEST(LmcLoadAtomicityChain, TruncatedTextureSectionRollsBackWithoutTouchingCurrentDocument) {
  TempFile f{ TempPath("lumice_lmc_atomicity_truncated_tex.lmc") };
  ASSERT_TRUE(WriteValidLmc(f.path, /*with_texture=*/true));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  ASSERT_GT(bytes.size(), kTexSizeField + 8u);
  const uint64_t tex_offset = ReadU64Field(bytes, kTexOffsetField);
  const uint64_t tex_size = ReadU64Field(bytes, kTexSizeField);
  ASSERT_GT(tex_size, 0u);
  ASSERT_EQ(bytes.size(), tex_offset + tex_size) << "premise: the texture section is the tail of the file";
  bytes.resize(bytes.size() - 1);  // one byte short — header still claims the full section
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "texture section truncated by one byte");
}

TEST(LmcLoadAtomicityChain, CorruptTexturePngRollsBackWithoutTouchingCurrentDocument) {
  TempFile f{ TempPath("lumice_lmc_atomicity_corrupt_png.lmc") };
  ASSERT_TRUE(WriteValidLmc(f.path, /*with_texture=*/true));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  const uint64_t tex_offset = ReadU64Field(bytes, kTexOffsetField);
  const uint64_t tex_size = ReadU64Field(bytes, kTexSizeField);
  ASSERT_GT(tex_size, 0u);
  ASSERT_GE(bytes.size(), tex_offset + tex_size);
  // Same length, so the section still READS in full; only the decode fails. This isolates the
  // third `return false` from the second one.
  for (uint64_t i = 0; i < tex_size; ++i) {
    bytes[static_cast<size_t>(tex_offset + i)] = 0xAB;
  }
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "texture section is not a decodable PNG");
}

// --- The JSON-section failure: a PIN, not a regression probe ----------------------------------
//
// This one was already atomic before the fix, and deliberately so is worth stating: within
// DeserializeGuiStateJson the ONLY `return false` is the catch around json::parse, and it sits
// ABOVE the `state = GuiState{}` line. So a JSON section that will not parse never reaches the
// overwrite. (The plan for this task assumed otherwise; reading the function settled it.)
//
// What this case buys is the ordering itself. "Return before you reset" is a property of two
// adjacent statements with nothing enforcing their order — move the reset above the try/catch, or
// add a second `return false` below it, and the atomicity this task just established for the
// texture section silently reopens on the JSON section. Green here today, red the day that
// happens.
//
// The payload is non-JSON bytes rather than well-formed-but-empty JSON on purpose: `{}` parses
// fine, so DeserializeGuiStateJson would return TRUE on it and the load would SUCCEED (into an
// empty document). That is a different proposition and not a failure path at all.
TEST(LmcLoadAtomicityChain, CorruptJsonPayloadRollsBackWithoutTouchingCurrentDocument) {
  TempFile f{ TempPath("lumice_lmc_atomicity_corrupt_json.lmc") };
  ASSERT_TRUE(WriteValidLmc(f.path, /*with_texture=*/true));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  const uint64_t json_offset = ReadU64Field(bytes, kJsonOffsetField);
  const uint64_t json_size = ReadU64Field(bytes, kJsonSizeField);
  ASSERT_GT(json_size, 0u);
  ASSERT_GE(bytes.size(), json_offset + json_size);
  // Same length, and the texture section stays valid, so the only thing wrong with this file is
  // that its JSON does not parse.
  for (uint64_t i = 0; i < json_size; ++i) {
    bytes[static_cast<size_t>(json_offset + i)] = '~';
  }
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "JSON section does not parse");
}

// --- The success path -------------------------------------------------------------------------
//
// Both texture modes, because "never touch the document" is satisfiable by a loader that never
// loads anything. These assert the replacement still happens, including the two fields the
// failure cases assert do NOT move.
//
// Field-level assertions rather than a whole-document JSON comparison against FileDocument(): the
// round trip's field-by-field fidelity is test_document_roundtrip_chain's subject, and restating
// it here would make this suite red for that suite's reasons.

TEST(LmcLoadAtomicityChain, SuccessfulLoadWithTextureFullyReplacesTheDocument) {
  TempFile f{ TempPath("lumice_lmc_atomicity_success_tex.lmc") };
  ASSERT_TRUE(WriteValidLmc(f.path, /*with_texture=*/true));

  GuiState current = SentinelDocument();
  LmcTexture tex;
  ASSERT_TRUE(LoadLmcFile(f.path, current, tex));

  ASSERT_EQ(current.crystals.size(), 1u);
  EXPECT_FLOAT_EQ(current.crystals[0].height.center, 2.5f) << "the file's document did not land";
  EXPECT_FLOAT_EQ(current.sun.altitude, 11.0f) << "the file's document did not land";
  EXPECT_TRUE(current.current_file_path.empty()) << "the sentinel's path survived a successful load";
  EXPECT_FALSE(current.dirty) << "the sentinel's dirty flag survived a successful load";

  EXPECT_TRUE(tex.HasPixels());
  EXPECT_EQ(tex.width, 4);
  EXPECT_EQ(tex.height, 4);
  EXPECT_EQ(tex.mode, PreviewRenderer::TextureMode::kSrgbRadiance);
  EXPECT_EQ(tex.srgb, TexturePixels());
  EXPECT_TRUE(tex.xyz.empty()) << "the sRGB and XYZ payloads are exclusive; a PNG section must not fill both";
}

TEST(LmcLoadAtomicityChain, SuccessfulLoadWithoutTextureFullyReplacesTheDocument) {
  TempFile f{ TempPath("lumice_lmc_atomicity_success_no_tex.lmc") };
  ASSERT_TRUE(WriteValidLmc(f.path, /*with_texture=*/false));

  GuiState current = SentinelDocument();
  LmcTexture tex;
  ASSERT_TRUE(LoadLmcFile(f.path, current, tex));

  ASSERT_EQ(current.crystals.size(), 1u);
  EXPECT_FLOAT_EQ(current.crystals[0].height.center, 2.5f) << "the file's document did not land";
  EXPECT_FLOAT_EQ(current.sun.altitude, 11.0f) << "the file's document did not land";
  EXPECT_TRUE(current.current_file_path.empty()) << "the sentinel's path survived a successful load";
  EXPECT_FALSE(current.dirty) << "the sentinel's dirty flag survived a successful load";
  EXPECT_FALSE(tex.HasPixels());
  EXPECT_TRUE(tex.srgb.empty());
  EXPECT_TRUE(tex.xyz.empty());
}

// The header field that says what the texture section MEANS, and the one pre-v4 answer that is
// still reachable.
//
// Why this proposition lives in the atomicity file rather than beside the JSON round trips: it is
// a claim about the BINARY container, and the header-offset constants and the byte-level edit
// helpers that make a claim like it checkable exist only here.
//
// What breaks without it. From v=4 the texture is the halo's radiance alone and the preview shader
// adds the lens's relative illumination and the sky at display time; up to v=3 the sky was summed
// into every texel by the writer and the shader can only draw those bytes as they are (subtracting
// it back out is not invertible where the bake clipped). One bit in the header separates the two,
// and reading it wrong paints the sky twice or not at all — a wrong PICTURE, with no error
// anywhere. The pre-v4 half is a path no writer produces any more, so nothing else exercises it.
TEST(LmcLoadAtomicityChain, TheHeaderSaysWhetherTheTextureCarriesTheSkyAndAPreV4FileSaysItDoes) {
  TempFile f{ TempPath("lumice_lmc_texture_semantics.lmc") };
  ASSERT_TRUE(WriteValidLmc(f.path, /*with_texture=*/true));

  GuiState state = SentinelDocument();
  LmcTexture tex;

  ASSERT_TRUE(LoadLmcFile(f.path, state, tex));
  EXPECT_EQ(tex.mode, PreviewRenderer::TextureMode::kSrgbRadiance)
      << "a file the 8-bit writer just produced must declare its texture radiance-only";

  // Now the same file as a pre-v4 writer would have left it: version 3, and the flag bit absent.
  // Both edits together, because either alone describes a file that never existed.
  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  ASSERT_GE(bytes.size(), static_cast<size_t>(44));
  const uint32_t flags = ReadU32Field(bytes, kFlagsField);
  ASSERT_NE(flags & 0x2u, 0u) << "premise broken: the writer did not set the radiance-only flag, so clearing it "
                                 "below tests nothing";
  WriteU32Field(bytes, kVersionField, 3);
  WriteU32Field(bytes, kFlagsField, flags & ~0x2u);
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  GuiState legacy_state = SentinelDocument();
  tex.mode = PreviewRenderer::TextureMode::kXyz;  // set to a WRONG value first, so a loader that never writes it fails
  ASSERT_TRUE(LoadLmcFile(f.path, legacy_state, tex));
  EXPECT_EQ(tex.mode, PreviewRenderer::TextureMode::kSrgbComposited)
      << "a pre-v4 file's texture has the sky baked in and must not be re-lit";
  EXPECT_EQ(tex.srgb, TexturePixels()) << "the pixels themselves are unchanged by the semantics flag";
}

// --- The v>=5 linear-XYZ texture section --------------------------------------------------------
//
// Byte offsets inside the XYZ texture section (v5 float32 and v6 float16 share the layout),
// restated here for the same reason the header offsets above are: the layout's single authority
// is the LmcXyzTextureHeader comment in src/gui/file_io.cpp, and its constants are file-static
// there.
constexpr size_t kXyzSectionHeaderSize = 32;
constexpr size_t kXyzSectionRawByteCountField = 24;
constexpr size_t kXyzSectionScaleField = 28;

void ExpectXyzMetaRoundTripped(const LmcTexture& tex) {
  const XyzTextureMeta m = XyzMeta();
  EXPECT_EQ(std::memcmp(&tex.meta.snapshot_intensity, &m.snapshot_intensity, sizeof(float)), 0);
  EXPECT_EQ(std::memcmp(&tex.meta.emitted_energy, &m.emitted_energy, sizeof(float)), 0);
  EXPECT_EQ(std::memcmp(&tex.meta.mono_anchor, &m.mono_anchor, sizeof(float)), 0);
  EXPECT_EQ(tex.meta.effective_pixels, m.effective_pixels);
}

// The point of the v6 encoding: what Open hands back is the writer's own quantization of what
// Save was handed — to the bit, and a fixed point of the codec, so re-quantizing it (which is what
// the GL upload does to it) lands on the same half bits the live frame's upload landed on. That
// fixed point, not float equality, is what "saved == reopened" means from v6 on, and it is exact,
// hence memcmp rather than EXPECT_FLOAT_EQ. The exposure measurements ride along untouched.
TEST(LmcLoadAtomicityChain, XyzHalfTextureRoundTripsToTheWritersOwnQuantizationWithItsExposureMeta) {
  TempFile f{ TempPath("lumice_lmc_xyz_roundtrip.lmc") };
  ASSERT_TRUE(WriteValidXyzLmc(f.path));

  // The header declares the encoding, and declares it on top of the display semantics rather than
  // instead of them: the decoder flag and the radiance-only flag are read by different code.
  const std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  ASSERT_GE(bytes.size(), 44u);
  EXPECT_EQ(ReadU32Field(bytes, kVersionField), 6u);
  const uint32_t flags = ReadU32Field(bytes, kFlagsField);
  EXPECT_EQ(flags & 0xBu, 0xBu) << "has_texture | radiance_only | xyz_half, all three";
  EXPECT_EQ(flags & 0x4u, 0u) << "xyz_float and xyz_half select different decoders and are exclusive";
  const uint64_t tex_offset = ReadU64Field(bytes, kTexOffsetField);
  ASSERT_GE(bytes.size(), tex_offset + kXyzSectionHeaderSize);
  EXPECT_EQ(ReadU32Field(bytes, static_cast<size_t>(tex_offset) + kXyzSectionRawByteCountField),
            4u * 4u * 3u * sizeof(uint16_t))
      << "the section declares its inflated size in binary16 units";
  const std::vector<float> px = XyzTexels();
  const float scale = ComputeXyzHalfScale(px.data(), px.size());
  EXPECT_EQ(std::memcmp(bytes.data() + tex_offset + kXyzSectionScaleField, &scale, sizeof(float)), 0)
      << "the header carries the codec's scale for these texels";

  GuiState state = SentinelDocument();
  LmcTexture tex;
  ASSERT_TRUE(LoadLmcFile(f.path, state, tex));
  EXPECT_FLOAT_EQ(state.sun.altitude, 11.0f) << "the file's document did not land";

  EXPECT_TRUE(tex.HasPixels());
  EXPECT_EQ(tex.width, 4);
  EXPECT_EQ(tex.height, 4);
  EXPECT_EQ(tex.mode, PreviewRenderer::TextureMode::kXyz);
  EXPECT_TRUE(tex.srgb.empty()) << "the sRGB and XYZ payloads are exclusive; a float section must not fill both";
  const std::vector<float> expected = XyzTexelsAsTheWriterStoresThem();
  ASSERT_EQ(tex.xyz.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(std::memcmp(&tex.xyz[i], &expected[i], sizeof(float)), 0)
        << "texel float " << i << " is not the writer's quantization, bit for bit";
  }
  // Premise of the proposition: the quantization did change the values (a 65x-over-white texel
  // with a fractional part cannot survive 11 significant bits), so "equal to the writer's
  // quantization" is a different statement from "equal to the input".
  bool any_moved = false;
  for (size_t i = 0; i < expected.size(); ++i) {
    any_moved = any_moved || std::memcmp(&expected[i], &px[i], sizeof(float)) != 0;
  }
  EXPECT_TRUE(any_moved) << "premise broken: float16 reproduced every input float, so this case cannot tell the "
                            "writer's quantization from the input";
  // The fixed point the GL upload of a reopened document relies on: re-quantizing what came back,
  // under the scale recomputed from it, lands on the bits the file holds.
  const float scale_again = ComputeXyzHalfScale(tex.xyz.data(), tex.xyz.size());
  EXPECT_EQ(std::memcmp(&scale_again, &scale, sizeof(float)), 0);
  std::vector<uint16_t> half_in(px.size());
  std::vector<uint16_t> half_back(px.size());
  QuantizeXyzToHalf(px.data(), px.size(), scale, half_in.data());
  QuantizeXyzToHalf(tex.xyz.data(), tex.xyz.size(), scale_again, half_back.data());
  EXPECT_EQ(half_in, half_back);
  ExpectXyzMetaRoundTripped(tex);
}

// The read-only half of the format: a v5 file's float32 section decodes to the floats it holds,
// bit for bit, through the decoder the v6 bump left in place. No writer produces this section, so
// the file is assembled by hand from the documented layout (RewriteAsV5FloatFile). The second
// assertion is the one that matters to the picture: quantizing those floats gives the same half
// bits the v6 writer stores for them, so a pre-v6 document opens to what a v6 re-save of it shows.
TEST(LmcLoadAtomicityChain, AV5FloatSectionStillDecodesBitExactAndQuantizesToWhatV6Stores) {
  TempFile f{ TempPath("lumice_lmc_xyz_v5_compat.lmc") };
  ASSERT_TRUE(WriteValidXyzLmc(f.path));
  ASSERT_TRUE(RewriteAsV5FloatFile(f.path));
  const std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  ASSERT_EQ(ReadU32Field(bytes, kVersionField), 5u);
  ASSERT_EQ(ReadU32Field(bytes, kFlagsField) & 0xCu, 0x4u) << "premise: xyz_float set, xyz_half clear";

  GuiState state = SentinelDocument();
  LmcTexture tex;
  ASSERT_TRUE(LoadLmcFile(f.path, state, tex));
  EXPECT_FLOAT_EQ(state.sun.altitude, 11.0f) << "the file's document did not land";
  EXPECT_EQ(tex.mode, PreviewRenderer::TextureMode::kXyz);
  EXPECT_EQ(tex.width, 4);
  EXPECT_EQ(tex.height, 4);
  const std::vector<float> px = XyzTexels();
  ASSERT_EQ(tex.xyz.size(), px.size());
  EXPECT_EQ(std::memcmp(tex.xyz.data(), px.data(), px.size() * sizeof(float)), 0)
      << "a v5 section's floats come back as stored, not requantized by the loader";
  ExpectXyzMetaRoundTripped(tex);

  std::vector<uint16_t> half_v5(px.size());
  QuantizeXyzToHalf(tex.xyz.data(), tex.xyz.size(), ComputeXyzHalfScale(tex.xyz.data(), tex.xyz.size()),
                    half_v5.data());
  const std::vector<float> v6 = XyzTexelsAsTheWriterStoresThem();
  std::vector<uint16_t> half_v6(px.size());
  QuantizeXyzToHalf(v6.data(), v6.size(), ComputeXyzHalfScale(v6.data(), v6.size()), half_v6.data());
  EXPECT_EQ(half_v5, half_v6) << "a reopened v5 document must upload the bits a v6 re-save of it holds";
}

// The decoder is selected by the flag, not inferred from the version. Clear the flag on a v6 file
// (leaving the version alone) and the loader must take the PNG path, which cannot read a deflate
// stream — and must fail the way every texture failure fails: with the open document untouched.
// A loader that guessed the encoding from the version, or from the bytes, would pass this file.
TEST(LmcLoadAtomicityChain, ClearingTheXyzFlagSendsTheSectionToThePngDecoderWhichRollsBack) {
  TempFile f{ TempPath("lumice_lmc_xyz_flag_cleared.lmc") };
  ASSERT_TRUE(WriteValidXyzLmc(f.path));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  const uint32_t flags = ReadU32Field(bytes, kFlagsField);
  ASSERT_NE(flags & 0x8u, 0u) << "premise broken: the writer did not set the xyz_half flag";
  WriteU32Field(bytes, kFlagsField, flags & ~0x8u);
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "v6 float16 section with the encoding flag cleared");
}

// And the other wrong decoder: the v5 flag on a v6 section. The float decoder expects twice the
// inflated bytes the header declares, so it must refuse — and roll back — rather than read half
// a texture as floats.
TEST(LmcLoadAtomicityChain, SwappingTheHalfFlagForTheFloatFlagRollsBack) {
  TempFile f{ TempPath("lumice_lmc_xyz_flag_swapped.lmc") };
  ASSERT_TRUE(WriteValidXyzLmc(f.path));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  const uint32_t flags = ReadU32Field(bytes, kFlagsField);
  ASSERT_NE(flags & 0x8u, 0u);
  WriteU32Field(bytes, kFlagsField, (flags & ~0x8u) | 0x4u);
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "v6 float16 section declared as v5 float32");
}

// The failure exit the float16 decoder adds on top of the float one: a scale that cannot have
// come from the codec (zero, which is also what a v5 writer left in those bytes).
TEST(LmcLoadAtomicityChain, ZeroScaleInAHalfSectionRollsBackWithoutTouchingCurrentDocument) {
  TempFile f{ TempPath("lumice_lmc_xyz_zero_scale.lmc") };
  ASSERT_TRUE(WriteValidXyzLmc(f.path));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  const uint64_t tex_offset = ReadU64Field(bytes, kTexOffsetField);
  ASSERT_GE(bytes.size(), tex_offset + kXyzSectionHeaderSize);
  const size_t field = static_cast<size_t>(tex_offset) + kXyzSectionScaleField;
  ASSERT_NE(ReadU32Field(bytes, field), 0u) << "premise: the writer wrote a non-zero scale";
  WriteU32Field(bytes, field, 0u);
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "v6 header scale is zero");
}

// The two failure exits the float decoder adds, each pinned like the PNG ones above: a header
// whose declared byte count disagrees with its dimensions, and a stream that does not inflate.
// Both keep the JSON section intact so the loader gets all the way past the deserializer first.
TEST(LmcLoadAtomicityChain, XyzHeaderByteCountMismatchRollsBackWithoutTouchingCurrentDocument) {
  TempFile f{ TempPath("lumice_lmc_xyz_bad_bytecount.lmc") };
  ASSERT_TRUE(WriteValidXyzLmc(f.path));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  const uint64_t tex_offset = ReadU64Field(bytes, kTexOffsetField);
  ASSERT_GE(bytes.size(), tex_offset + kXyzSectionHeaderSize);
  const size_t field = static_cast<size_t>(tex_offset) + kXyzSectionRawByteCountField;
  ASSERT_EQ(ReadU32Field(bytes, field), 4u * 4u * 3u * sizeof(uint16_t)) << "premise: the writer declared 4x4x3 halves";
  WriteU32Field(bytes, field, 4u * 4u * 3u * sizeof(uint16_t) - 2u);
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "v6 header raw_byte_count disagrees with width*height");
}

TEST(LmcLoadAtomicityChain, CorruptXyzZlibStreamRollsBackWithoutTouchingCurrentDocument) {
  TempFile f{ TempPath("lumice_lmc_xyz_corrupt_stream.lmc") };
  ASSERT_TRUE(WriteValidXyzLmc(f.path));

  std::vector<unsigned char> bytes = ReadAllBytes(f.path);
  const uint64_t tex_offset = ReadU64Field(bytes, kTexOffsetField);
  const uint64_t tex_size = ReadU64Field(bytes, kTexSizeField);
  ASSERT_GT(tex_size, kXyzSectionHeaderSize);
  ASSERT_GE(bytes.size(), tex_offset + tex_size);
  // Header left intact, so dimensions and byte count still agree; only the stream is garbage.
  for (uint64_t i = kXyzSectionHeaderSize; i < tex_size; ++i) {
    bytes[static_cast<size_t>(tex_offset + i)] = 0xAB;
  }
  ASSERT_TRUE(WriteAllBytes(f.path, bytes));

  ExpectLoadFailsAndDocumentUnchanged(f.path, "v6 zlib stream does not inflate");
}

}  // namespace
}  // namespace lumice::gui
