#include "crypto_string.h"

#include <sodium.h>

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {

std::once_flag g_sodium_once;
bool g_sodium_ok = false;

// Replace this with your real fixed secret.
// IMPORTANT: This must be exactly crypto_secretbox_KEYBYTES bytes after decoding.
// Best practice is to store a 32-byte key as base64 and decode it once.
// Randomly generated:
static const char *kSecretKeyB64 = "Wc9CqkZq8W1gk9vXj2K1Yc7Fh6pQnT3mLr5uD0aB8Hs=";

bool ensure_sodium_init()
{
    std::call_once(g_sodium_once, []() {
        g_sodium_ok = (sodium_init() >= 0);
    });
    return g_sodium_ok;
}

bool load_key(unsigned char key[crypto_secretbox_KEYBYTES])
{
    if (!ensure_sodium_init()) {
        return false;
    }

    size_t key_len = 0;
    if (sodium_base642bin(key,
                          crypto_secretbox_KEYBYTES,
                          kSecretKeyB64,
                          std::strlen(kSecretKeyB64),
                          nullptr,
                          &key_len,
                          nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0) {
        return false;
    }

    return key_len == crypto_secretbox_KEYBYTES;
}

char *dup_string(const std::string &s)
{
    char *out = static_cast<char *>(std::malloc(s.size() + 1));
    if (!out) {
        return nullptr;
    }
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

} // namespace

extern "C" const char *taffylabs_encrypt_string(const char *plain_text)
{
    if (!plain_text) {
        return nullptr;
    }

    unsigned char key[crypto_secretbox_KEYBYTES];
    if (!load_key(key)) {
        return nullptr;
    }

    const auto *msg = reinterpret_cast<const unsigned char *>(plain_text);
    const unsigned long long msg_len =
        static_cast<unsigned long long>(std::strlen(plain_text));

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    std::vector<unsigned char> cipher(
        crypto_secretbox_MACBYTES + msg_len);

    if (crypto_secretbox_easy(cipher.data(), msg, msg_len, nonce, key) != 0) {
        sodium_memzero(key, sizeof(key));
        return nullptr;
    }

    // Store [nonce || cipher] together, then base64 encode for portability.
    std::vector<unsigned char> packed(sizeof(nonce) + cipher.size());
    std::memcpy(packed.data(), nonce, sizeof(nonce));
    std::memcpy(packed.data() + sizeof(nonce), cipher.data(), cipher.size());

    const size_t b64_len =
        sodium_base64_encoded_len(packed.size(), sodium_base64_VARIANT_ORIGINAL);

    std::string encoded(b64_len, '\0');
    sodium_bin2base64(encoded.data(),
                      encoded.size(),
                      packed.data(),
                      packed.size(),
                      sodium_base64_VARIANT_ORIGINAL);

    // Remove trailing null from std::string view if present.
    if (!encoded.empty() && encoded.back() == '\0') {
        encoded.pop_back();
    }

    sodium_memzero(key, sizeof(key));
    return dup_string(encoded);
}

extern "C" const char *taffylabs_decrypt_string(const char *encoded_cipher_text)
{
    if (!encoded_cipher_text) {
        return nullptr;
    }

    unsigned char key[crypto_secretbox_KEYBYTES];
    if (!load_key(key)) {
        return nullptr;
    }

    std::vector<unsigned char> packed(std::strlen(encoded_cipher_text));
    size_t packed_len = 0;

    if (sodium_base642bin(packed.data(),
                          packed.size(),
                          encoded_cipher_text,
                          std::strlen(encoded_cipher_text),
                          nullptr,
                          &packed_len,
                          nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0) {
        sodium_memzero(key, sizeof(key));
        return nullptr;
    }

    if (packed_len < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        sodium_memzero(key, sizeof(key));
        return nullptr;
    }

    const unsigned char *nonce = packed.data();
    const unsigned char *cipher = packed.data() + crypto_secretbox_NONCEBYTES;
    const unsigned long long cipher_len =
        static_cast<unsigned long long>(packed_len - crypto_secretbox_NONCEBYTES);

    if (cipher_len < crypto_secretbox_MACBYTES) {
        sodium_memzero(key, sizeof(key));
        return nullptr;
    }

    std::vector<unsigned char> plain(cipher_len - crypto_secretbox_MACBYTES);

    if (crypto_secretbox_open_easy(
            plain.data(), cipher, cipher_len, nonce, key) != 0) {
        sodium_memzero(key, sizeof(key));
        return nullptr;
    }

    std::string out(reinterpret_cast<const char *>(plain.data()), plain.size());

    sodium_memzero(key, sizeof(key));
    return dup_string(out);
}

extern "C" void crypto_string_free(const char *p)
{
    std::free(const_cast<char *>(p));
}