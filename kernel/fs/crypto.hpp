#ifndef CRYPTO_HPP
#define CRYPTO_HPP

#include <stdint.h>
#include <stddef.h>

namespace MesaOS::FS::Crypto {

// AES-128 encryption/decryption for filesystem
class AES128 {
public:
    static void encrypt_block(uint8_t* block, const uint8_t* key);
    static void decrypt_block(uint8_t* block, const uint8_t* key);
    static void encrypt_buffer(uint8_t* buffer, size_t size, const uint8_t* key);
    static void decrypt_buffer(uint8_t* buffer, size_t size, const uint8_t* key);

private:
    static void key_expansion(const uint8_t* key, uint8_t* round_keys);
    static void add_round_key(uint8_t* state, const uint8_t* round_key);
    static void sub_bytes(uint8_t* state);
    static void inv_sub_bytes(uint8_t* state);
    static void shift_rows(uint8_t* state);
    static void inv_shift_rows(uint8_t* state);
    static void mix_columns(uint8_t* state);
    static void inv_mix_columns(uint8_t* state);
    static uint8_t xtime(uint8_t x);
    static uint8_t multiply(uint8_t x, uint8_t y);
};

// HMAC-SHA256 for integrity verification
class HMAC_SHA256 {
public:
    static void compute(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, uint8_t* hmac_out);
    static bool verify(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, const uint8_t* expected_hmac);

private:
    static void sha256_init(uint32_t* state);
    static void sha256_transform(uint32_t* state, const uint8_t* data);
    static void sha256_final(uint32_t* state, uint8_t* hash, uint64_t total_len);
};

// Authenticated encryption using AES-128 + HMAC
class AuthenticatedAES {
public:
    static bool encrypt_buffer(uint8_t* buffer, size_t size, const uint8_t* key, uint8_t* hmac_out);
    static bool decrypt_buffer(uint8_t* buffer, size_t size, const uint8_t* key, const uint8_t* expected_hmac);
    static const size_t HMAC_SIZE = 32; // SHA256 produces 32-byte HMAC
};

// Key management for encrypted filesystem
class KeyManager {
public:
    static void initialize();
    static const uint8_t* get_master_key();
    static const uint8_t* get_hmac_key();
    static void derive_key(const char* password, uint8_t* key_out);
    static void derive_keys_from_password(const char* password);

private:
    static uint8_t master_key[16];
    static uint8_t hmac_key[32]; // 256-bit key for HMAC
    static bool initialized;
};

} // namespace MesaOS::FS::Crypto

#endif
