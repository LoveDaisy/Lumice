#include "raypath/detail/survey/config_fingerprint.hpp"

#include <nlohmann/json.hpp>

#include "util/fatal.hpp"

namespace lumice::raypath::survey {
namespace {

constexpr const char* kAxisSetTag = "lumice.routing-fingerprint-v1";

// ---------------------------------------------------------------------------
// SHA-256 (FIPS 180-4), self-contained: the fingerprint's only cryptographic
// need, small enough that a dependency for it would out-weigh the code. Hex
// output, lower case.
// ---------------------------------------------------------------------------

constexpr uint32_t kK[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

uint32_t Rotr(uint32_t x, int n) {
  return (x >> n) | (x << (32 - n));
}

void Sha256(const std::string& message, std::string* hex_out) {
  uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
  // Padded message: the input, one 0x80, zeros, then the 64-bit big-endian bit length.
  std::vector<uint8_t> data(message.begin(), message.end());
  const uint64_t bits = static_cast<uint64_t>(data.size()) * 8;
  data.push_back(0x80);
  while (data.size() % 64 != 56) {
    data.push_back(0x00);
  }
  for (int i = 7; i >= 0; i--) {
    data.push_back(static_cast<uint8_t>((bits >> (8 * i)) & 0xff));
  }
  for (size_t block = 0; block < data.size(); block += 64) {
    uint32_t w[64];
    for (int t = 0; t < 16; t++) {
      const size_t o = block + 4 * static_cast<size_t>(t);
      w[t] = (static_cast<uint32_t>(data[o]) << 24) | (static_cast<uint32_t>(data[o + 1]) << 16) |
             (static_cast<uint32_t>(data[o + 2]) << 8) | static_cast<uint32_t>(data[o + 3]);
    }
    for (int t = 16; t < 64; t++) {
      const uint32_t s0 = Rotr(w[t - 15], 7) ^ Rotr(w[t - 15], 18) ^ (w[t - 15] >> 3);
      const uint32_t s1 = Rotr(w[t - 2], 17) ^ Rotr(w[t - 2], 19) ^ (w[t - 2] >> 10);
      w[t] = w[t - 16] + s0 + w[t - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int t = 0; t < 64; t++) {
      const uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
      const uint32_t ch = (e & f) ^ (~e & g);
      const uint32_t t1 = hh + s1 + ch + kK[t] + w[t];
      const uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
      const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t t2 = s0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }
  static const char* kHexDigits = "0123456789abcdef";
  hex_out->clear();
  hex_out->reserve(64);
  for (const uint32_t word : h) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      hex_out->push_back(kHexDigits[(word >> shift) & 0xf]);
    }
  }
}

}  // namespace

std::string Sha256Hex(const std::string& message) {
  std::string hex;
  Sha256(message, &hex);
  return hex;
}

std::string RoutingConfigFingerprint(const ConfigManager& config, uint64_t crystal_id,
                                     const std::vector<std::vector<int>>& path_layers, uint8_t symmetry_bits) {
  // nlohmann::json (not ordered_json): object keys sort, so the same data always dumps to the
  // same bytes. The crystal entry's own to_json is the config's single serialization — the
  // fingerprint never re-spells a shape field.
  nlohmann::json projection;
  projection["axis_set"] = kAxisSetTag;
  const auto entry = config.crystals_.find(static_cast<IdType>(crystal_id));
  // The caller has gated the crystal id (the survey request validated it; the point bridge
  // refuses an unknown id before computing the fingerprint): a miss here is a caller bug, and hashing a projection that
  // silently omits the crystal would be worse than refusing to answer.
  if (entry == config.crystals_.end()) {
    FatalAbort("RoutingConfigFingerprint: crystal %llu is not in the config (caller bug: the id was not gated)",
               static_cast<unsigned long long>(crystal_id));
  }
  {
    nlohmann::json crystal;
    to_json(crystal, entry->second);
    projection["crystal"] = std::move(crystal);
  }
  projection["path_layers"] = path_layers;
  projection["symmetry_bits"] = symmetry_bits;
  std::string hex;
  Sha256(projection.dump(), &hex);
  return hex;
}

}  // namespace lumice::raypath::survey
