#include "aes_string.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
	static constexpr const char *kPrefix = "AES1:";
	static constexpr int kIvSize = 16;
	static constexpr int kKeySize = 32;

	// 🔐 RANDOMLY GENERATED 256-BIT KEY (DO NOT CHANGE AFTER SHIPPING)
	static const unsigned char kKey[kKeySize] = {
		0x7a, 0x13, 0x5c, 0x91, 0x2e, 0xb4, 0x88, 0x6f,
		0x4a, 0xd9, 0x01, 0x73, 0xfe, 0x2c, 0x55, 0x99,
		0x3b, 0x6e, 0x80, 0x11, 0xa7, 0xcd, 0x42, 0xef,
		0x90, 0x24, 0x5a, 0x77, 0x18, 0x6b, 0xd2, 0x3c};

	static char *dup_cstr(const std::string &s)
	{
		char *out = (char *)std::malloc(s.size() + 1);
		if (!out)
			return nullptr;

		std::memcpy(out, s.c_str(), s.size() + 1);
		return out;
	}

	static std::string base64_encode(const unsigned char *data, size_t len)
	{
		if (!data || len == 0)
			return {};

		int out_len = 4 * ((int)len + 2) / 3;
		std::string out;
		out.resize(out_len);

		int actual = EVP_EncodeBlock(
			(unsigned char *)&out[0],
			data,
			(int)len);

		if (actual <= 0)
			return {};

		out.resize(actual);
		return out;
	}

	static bool base64_decode(const std::string &input, std::vector<unsigned char> &out)
	{
		out.clear();

		if (input.empty())
			return false;

		std::vector<unsigned char> buffer((input.size() * 3) / 4 + 3);

		int actual = EVP_DecodeBlock(
			buffer.data(),
			(const unsigned char *)input.data(),
			(int)input.size());

		if (actual < 0)
			return false;

		int decoded_len = actual;

		size_t padding = 0;
		if (input.back() == '=') padding++;
		if (input.size() > 1 && input[input.size() - 2] == '=') padding++;

		decoded_len -= (int)padding;
		if (decoded_len < 0)
			return false;

		buffer.resize(decoded_len);
		out = std::move(buffer);
		return true;
	}
}

extern "C" char *taffy_aes_encrypt_string(const char *plaintext)
{
	if (!plaintext)
		return nullptr;

	unsigned char iv[kIvSize];

	if (RAND_bytes(iv, sizeof(iv)) != 1)
		return nullptr;

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return nullptr;

	int plaintext_len = (int)std::strlen(plaintext);

	std::vector<unsigned char> ciphertext(
		plaintext_len + EVP_MAX_BLOCK_LENGTH);

	int out_len1 = 0;
	int out_len2 = 0;
	bool ok = false;

	do {
		if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, kKey, iv) != 1)
			break;

		if (EVP_EncryptUpdate(
			    ctx,
			    ciphertext.data(),
			    &out_len1,
			    (const unsigned char *)plaintext,
			    plaintext_len) != 1)
			break;

		if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len1, &out_len2) != 1)
			break;

		ciphertext.resize(out_len1 + out_len2);
		ok = true;

	} while (false);

	EVP_CIPHER_CTX_free(ctx);

	if (!ok)
		return nullptr;

	std::vector<unsigned char> packed;
	packed.reserve(kIvSize + ciphertext.size());

	packed.insert(packed.end(), iv, iv + kIvSize);
	packed.insert(packed.end(), ciphertext.begin(), ciphertext.end());

	std::string b64 = base64_encode(packed.data(), packed.size());
	if (b64.empty())
		return nullptr;

	return dup_cstr(std::string(kPrefix) + b64);
}

extern "C" char *taffy_aes_decrypt_string(const char *encrypted)
{
	if (!encrypted)
		return nullptr;

	size_t prefix_len = std::strlen(kPrefix);
	if (std::strncmp(encrypted, kPrefix, prefix_len) != 0)
		return nullptr;

	std::string b64 = encrypted + prefix_len;

	std::vector<unsigned char> packed;
	if (!base64_decode(b64, packed))
		return nullptr;

	if (packed.size() < (size_t)kIvSize + 1)
		return nullptr;

	const unsigned char *iv = packed.data();
	const unsigned char *ciphertext = packed.data() + kIvSize;
	int ciphertext_len = (int)(packed.size() - kIvSize);

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return nullptr;

	std::vector<unsigned char> plaintext_buf(
		ciphertext_len + EVP_MAX_BLOCK_LENGTH);

	int out_len1 = 0;
	int out_len2 = 0;
	bool ok = false;

	do {
		if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, kKey, iv) != 1)
			break;

		if (EVP_DecryptUpdate(
			    ctx,
			    plaintext_buf.data(),
			    &out_len1,
			    ciphertext,
			    ciphertext_len) != 1)
			break;

		if (EVP_DecryptFinal_ex(ctx, plaintext_buf.data() + out_len1, &out_len2) != 1)
			break;

		plaintext_buf.resize(out_len1 + out_len2);
		ok = true;

	} while (false);

	EVP_CIPHER_CTX_free(ctx);

	if (!ok)
		return nullptr;

	std::string plaintext(
		(const char *)plaintext_buf.data(),
		plaintext_buf.size());

	return dup_cstr(plaintext);
}

extern "C" void taffy_aes_free(char *ptr)
{
	if (ptr)
		std::free(ptr);
}