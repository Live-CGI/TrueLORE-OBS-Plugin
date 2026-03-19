#pragma once

#ifdef __cplusplus
extern "C" {
#endif

char *taffy_aes_encrypt_string(const char *plaintext);
char *taffy_aes_decrypt_string(const char *encrypted);

void taffy_aes_free(char *ptr);

#ifdef __cplusplus
}
#endif