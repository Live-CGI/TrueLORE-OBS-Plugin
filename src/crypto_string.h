#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Returns a newly allocated base64 string containing:
// [nonce || ciphertext+mac]
// Caller must free with crypto_string_free().
const char *taffylabs_encrypt_string(const char *plain_text);

// Returns a newly allocated UTF-8/plain string on success.
// Returns nullptr on failure.
// Caller must free with crypto_string_free().
const char *taffylabs_decrypt_string(const char *encoded_cipher_text);

// Frees strings returned by this module.
void crypto_string_free(const char *p);

#ifdef __cplusplus
}
#endif