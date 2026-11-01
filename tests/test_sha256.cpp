// Tests of TinySHA256 and TinyHMACSHA256 with the official test vectors
// (FIPS 180-4, RFC 4231) and values from python's hashlib/hmac.
#include <string>

#include "TinyNTP/TinySHA256.h"
#include "test.h"

using namespace tiny_ntp;

static std::string sha(const std::string& data) {
  uint8_t d[32];
  TinySHA256 h;
  h.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
  h.finish(d);
  char out[65];
  for (int i = 0; i < 32; i++) snprintf(out + 2 * i, 3, "%02x", d[i]);
  return out;
}

static std::string mac(const std::string& key, const std::string& data) {
  uint8_t d[32];
  TinyHMACSHA256::calculate(reinterpret_cast<const uint8_t*>(key.data()),
                            key.size(),
                            reinterpret_cast<const uint8_t*>(data.data()),
                            data.size(), d);
  char out[65];
  for (int i = 0; i < 32; i++) snprintf(out + 2 * i, 3, "%02x", d[i]);
  return out;
}

static std::string pattern(size_t len, int mul, int add) {
  std::string s;
  for (size_t i = 0; i < len; i++) s += static_cast<char>((i * mul + add) % 256);
  return s;
}

TEST(sha256_vectors) {
  CHECK_STR(sha(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");  // empty
  CHECK_STR(sha("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");  // abc
  CHECK_STR(sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");  // two_blocks
  CHECK_STR(sha(std::string(1000000, 'a')),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(sha256_padding_boundaries) {
  CHECK_STR(sha(std::string(54, 'x')), "45f316e10b2c99abf374b22bda893cf3300d77263f1e272349ed414680522952");
  CHECK_STR(sha(std::string(55, 'x')), "d5e285683cd4efc02d021a5c62014694958901005d6f71e89e0989fac77e4072");
  CHECK_STR(sha(std::string(56, 'x')), "04c26261370ee7541549d16dee320c723e3fd14671e66a099afe0a377c16888e");
  CHECK_STR(sha(std::string(57, 'x')), "ae14a2563ccf969d99aca69ce6bb74981f734bbf9f655f73b8f06db68cab5217");
  CHECK_STR(sha(std::string(63, 'x')), "75220b47218278e656f2013bb8f0c455a25eaf01e86c64924e9d48d89776d6f2");
  CHECK_STR(sha(std::string(64, 'x')), "7ce100971f64e7001e8fe5a51973ecdfe1ced42befe7ee8d5fd6219506b5393c");
  CHECK_STR(sha(std::string(65, 'x')), "9537c5fdf120482f7d58d25e9ed583f52c02b4e304ea814db1633ad565aed7e9");
  CHECK_STR(sha(std::string(119, 'x')), "000b48d4edf0fa7bee3c6236ecd2785baa5db4eeb8bb54341b029e0d9fa5fb0c");
  CHECK_STR(sha(std::string(120, 'x')), "13f05a0b594787f5ecd315edc96141bd3243203d1b7d4f0836f37308b276ba98");
  CHECK_STR(sha(std::string(128, 'x')), "24da1b81d0b16df6428eee73c69fcb2a93c76bc6df706f0c6670fe6bfe800464");
}

TEST(sha256_incremental_equals_single_update) {
  std::string data = pattern(1000, 7, 3);
  for (size_t chunk : {1, 3, 7, 63, 64, 65, 500}) {
    uint8_t d[32];
    TinySHA256 h;
    for (size_t i = 0; i < data.size(); i += chunk) {
      size_t n = data.size() - i < chunk ? data.size() - i : chunk;
      h.update(reinterpret_cast<const uint8_t*>(data.data()) + i, n);
    }
    h.finish(d);
    char out[65];
    for (int i = 0; i < 32; i++) snprintf(out + 2 * i, 3, "%02x", d[i]);
    CHECK_STR(std::string(out), sha(data));
  }
}

TEST(sha256_reset_starts_a_new_hash) {
  TinySHA256 h;
  uint8_t d[32];
  h.update(reinterpret_cast<const uint8_t*>("garbage"), 7);
  h.reset();
  h.update(reinterpret_cast<const uint8_t*>("abc"), 3);
  h.finish(d);
  CHECK_EQ(d[0], 0xba);
  CHECK_EQ(d[31], 0xad);
}

TEST(hmac_rfc4231) {
  CHECK_STR(mac(std::string(20, '\x0b'), "Hi There"),
            "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
  CHECK_STR(mac("Jefe", "what do ya want for nothing?"),
            "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
  CHECK_STR(mac(std::string(20, '\xaa'), std::string(50, '\xdd')),
            "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
  // key longer than the block size is hashed first
  CHECK_STR(mac(std::string(131, '\xaa'),
                "Test Using Larger Than Block-Size Key - Hash Key First"),
            "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
}

TEST(hmac_key_and_data_lengths) {
  CHECK_STR(mac(pattern(0, 7, 3), pattern(0, 5, 1)), "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad");
  CHECK_STR(mac(pattern(0, 7, 3), pattern(1, 5, 1)), "3d7afb663124ecbf2c953f863d4fc8796eeb2d372b64aad58697ec5264649cdb");
  CHECK_STR(mac(pattern(0, 7, 3), pattern(64, 5, 1)), "be6286444441476de37795601d366368613befb07e85ba8aa2f6749623b2b7a0");
  CHECK_STR(mac(pattern(0, 7, 3), pattern(100, 5, 1)), "c073a59d641459acff4561f9151534b7e395fab42a1cbda58021a5dff4dc09b6");
  CHECK_STR(mac(pattern(1, 7, 3), pattern(0, 5, 1)), "be8d44d01aa938627809668b7364a21ed8e9fc8cffa64d18846d126789d4cef8");
  CHECK_STR(mac(pattern(1, 7, 3), pattern(1, 5, 1)), "75950556b2e34fca34ff21f80c7316fee7387c6a9c0e73b09afb0e3302ffe067");
  CHECK_STR(mac(pattern(1, 7, 3), pattern(64, 5, 1)), "6895331dd7264dd25f621368ba3e5f4ae3ac49a08ef291be6c21b77214731e08");
  CHECK_STR(mac(pattern(1, 7, 3), pattern(100, 5, 1)), "2bac9023e81e072e743127e6e2f8840b2cc02fae1ed1bb7eebd0e4cc128118a6");
  CHECK_STR(mac(pattern(32, 7, 3), pattern(0, 5, 1)), "feffb2318c2191c4f11e1fc0ee3d808697d6073ae8a1c30be5f1a169ceff13e6");
  CHECK_STR(mac(pattern(32, 7, 3), pattern(1, 5, 1)), "1887a7e7c9c15938b942006b573e26f129166114a170fddc6f9ca7230d68406d");
  CHECK_STR(mac(pattern(32, 7, 3), pattern(64, 5, 1)), "a990e28e9eb416e8d2d066cca968f1db817e1f64199b22057f3ce13c2853e548");
  CHECK_STR(mac(pattern(32, 7, 3), pattern(100, 5, 1)), "7983f242c578c29f4ed8964b4bc2bc5507c5c4890648244f14a30c4d5f92549a");
  CHECK_STR(mac(pattern(63, 7, 3), pattern(0, 5, 1)), "646aefd3a921995422ef7683d9726a66715f7e764d7e2211478f1c19b2ca13c8");
  CHECK_STR(mac(pattern(63, 7, 3), pattern(1, 5, 1)), "3b06c2982fea14a420ea001d3e4c0310cbe5d07467ad1db856d6009963772b9b");
  CHECK_STR(mac(pattern(63, 7, 3), pattern(64, 5, 1)), "e73f7ddad11950ed7501defb6d1079e200f7f6437328d0bf55236e86601f62e0");
  CHECK_STR(mac(pattern(63, 7, 3), pattern(100, 5, 1)), "2100b84090fe4701f738e197f26322111631fab768d672860056e33e33dfe633");
  CHECK_STR(mac(pattern(64, 7, 3), pattern(0, 5, 1)), "58c7e63111455c2bf96038a7f89d0b8003da9a940571640f70d14f4f3f983fc1");
  CHECK_STR(mac(pattern(64, 7, 3), pattern(1, 5, 1)), "3eb2651f742a1a4bc82773c59014ddf1575a9bd1480475d35ff3449b3306de45");
  CHECK_STR(mac(pattern(64, 7, 3), pattern(64, 5, 1)), "0202f5d870b6d225a85e6d201867b7da15784f041f995592a69dd28d0c0c8136");
  CHECK_STR(mac(pattern(64, 7, 3), pattern(100, 5, 1)), "7c0216ed49a0b881bb605f750ffc162f76539053da2f3b6706caf19ebb1238f5");
  CHECK_STR(mac(pattern(65, 7, 3), pattern(0, 5, 1)), "34bb3ae87adcec2d42a585be6634ffec56e8cf0caac667a4e398ee1e46015c15");
  CHECK_STR(mac(pattern(65, 7, 3), pattern(1, 5, 1)), "5381ef8009433c8bfd175bd9c24d9b4404b3ff441ad90db599af9c06aa689b47");
  CHECK_STR(mac(pattern(65, 7, 3), pattern(64, 5, 1)), "cf524f6838f91bab31a3450b67fe9c15b69105c323554cb8a4c57abdef98097f");
  CHECK_STR(mac(pattern(65, 7, 3), pattern(100, 5, 1)), "50a050a438767a39dbda967dc59b1e18a6adb6bf62ffbf21dfb8383fa7c909dc");
  CHECK_STR(mac(pattern(200, 7, 3), pattern(0, 5, 1)), "124f0c19ba870c7b73e1e583911fb285fd7513db68f6bb116b1e34a66f4b6181");
  CHECK_STR(mac(pattern(200, 7, 3), pattern(1, 5, 1)), "7018324bf1a10dae3785145fb9a98cd8815dc3dbb7a45453317898609e46b216");
  CHECK_STR(mac(pattern(200, 7, 3), pattern(64, 5, 1)), "2384aa9d441e55e3199506e0563bc4f2ffcf8f412dcbd9dfed9da9ec39c7a756");
  CHECK_STR(mac(pattern(200, 7, 3), pattern(100, 5, 1)), "46ee0f59a0fc158e349c5ed3596cdcbc652877ec76a36d1a843ac8a3f8d65d2e");
}

TEST(hmac_equals_compares_all_bytes) {
  uint8_t a[32], b[32];
  for (int i = 0; i < 32; i++) a[i] = b[i] = i;
  CHECK(TinyHMACSHA256::equals(a, b, 32));
  for (int i = 0; i < 32; i++) {
    b[i] ^= 1;
    CHECK(!TinyHMACSHA256::equals(a, b, 32));  // a difference at any position
    b[i] ^= 1;
  }
  CHECK(TinyHMACSHA256::equals(a, b, 0));
}

TEST_MAIN("sha256")
