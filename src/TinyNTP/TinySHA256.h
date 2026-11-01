/**
 * @file TinySHA256.h
 * @brief Small, portable SHA-256 and HMAC-SHA256 (FIPS 180-4, RFC 2104).
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tiny_ntp {

/**
 * @class TinySHA256
 * @brief SHA-256 hash. Call update() for the data and finish() for the digest.
 * It needs about 110 bytes of RAM and no heap.
 */
class TinySHA256 {
 public:
  /** Size of the digest in bytes. */
  static constexpr size_t DigestSize = 32;
  /** Size of a block in bytes. */
  static constexpr size_t BlockSize = 64;

  TinySHA256() { reset(); }

  /** @brief Start a new hash. */
  void reset() {
    static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                     0xa54ff53a, 0x510e527f, 0x9b05688c,
                                     0x1f83d9ab, 0x5be0cd19};
    memcpy(_state, init, sizeof _state);
    _bitCount = 0;
    _bufferLen = 0;
  }

  /** @brief Add data to the hash. */
  void update(const uint8_t* data, size_t len) {
    _bitCount += static_cast<uint64_t>(len) * 8;
    while (len > 0) {
      size_t n = BlockSize - _bufferLen;
      if (n > len) n = len;
      memcpy(_buffer + _bufferLen, data, n);
      _bufferLen += n;
      data += n;
      len -= n;
      if (_bufferLen == BlockSize) {
        transform(_buffer);
        _bufferLen = 0;
      }
    }
  }

  /**
   * @brief Finish the hash. Call reset() before you use the object again.
   * @param digest Receives the 32 byte digest.
   */
  void finish(uint8_t* digest) {
    uint64_t bits = _bitCount;
    uint8_t pad = 0x80;
    update(&pad, 1);
    pad = 0;
    while (_bufferLen != 56) update(&pad, 1);
    uint8_t length[8];
    for (int i = 0; i < 8; i++) length[i] = bits >> (56 - 8 * i);
    update(length, 8);
    for (int i = 0; i < 8; i++) {
      digest[i * 4] = _state[i] >> 24;
      digest[i * 4 + 1] = _state[i] >> 16;
      digest[i * 4 + 2] = _state[i] >> 8;
      digest[i * 4 + 3] = _state[i];
    }
  }

 protected:
  /** The hash state. */
  uint32_t _state[8];
  /** Number of bits that were added. */
  uint64_t _bitCount;
  /** Data of the current block. */
  uint8_t _buffer[BlockSize];
  /** Number of bytes in the current block. */
  size_t _bufferLen;

  /** Rotate a 32 bit value to the right. */
  static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

  /** Process one block of 64 bytes. */
  void transform(const uint8_t* block) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
      w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
             (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = _state[0], b = _state[1], c = _state[2], d = _state[3];
    uint32_t e = _state[4], f = _state[5], g = _state[6], h = _state[7];
    for (int i = 0; i < 64; i++) {
      uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = h + s1 + ch + k[i] + w[i];
      uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = s0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    _state[0] += a;
    _state[1] += b;
    _state[2] += c;
    _state[3] += d;
    _state[4] += e;
    _state[5] += f;
    _state[6] += g;
    _state[7] += h;
  }
};

/**
 * @class TinyHMACSHA256
 * @brief Keyed hash (HMAC-SHA256) to authenticate messages with a shared secret.
 */
class TinyHMACSHA256 {
 public:
  /**
   * @brief Calculate the HMAC of one message.
   * @param key The secret.
   * @param keyLen Length of the secret in bytes.
   * @param data The message.
   * @param dataLen Length of the message in bytes.
   * @param mac Receives the 32 byte HMAC.
   */
  static void calculate(const uint8_t* key, size_t keyLen, const uint8_t* data,
                        size_t dataLen, uint8_t* mac) {
    uint8_t k[TinySHA256::BlockSize] = {};
    if (keyLen > TinySHA256::BlockSize) {
      TinySHA256 h;
      h.update(key, keyLen);
      h.finish(k);
    } else {
      memcpy(k, key, keyLen);
    }
    uint8_t pad[TinySHA256::BlockSize];
    uint8_t inner[TinySHA256::DigestSize];
    TinySHA256 h;
    for (size_t i = 0; i < sizeof pad; i++) pad[i] = k[i] ^ 0x36;
    h.update(pad, sizeof pad);
    h.update(data, dataLen);
    h.finish(inner);
    h.reset();
    for (size_t i = 0; i < sizeof pad; i++) pad[i] = k[i] ^ 0x5c;
    h.update(pad, sizeof pad);
    h.update(inner, sizeof inner);
    h.finish(mac);
  }

  /**
   * @brief Compare two MACs in constant time (no information about the
   * position of the first difference).
   * @param a First MAC.
   * @param b Second MAC.
   * @param len Number of bytes.
   * @return true if they are the same.
   */
  static bool equals(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= a[i] ^ b[i];
    return diff == 0;
  }
};

}  // namespace tiny_ntp
