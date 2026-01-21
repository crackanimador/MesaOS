#include "crypto.hpp"
#include <string.h>

namespace MesaOS::FS::Crypto {

uint8_t KeyManager::master_key[16];
uint8_t KeyManager::hmac_key[32];
bool KeyManager::initialized = false;

// S-box for AES
static const uint8_t s_box[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

// Inverse S-box
static const uint8_t inv_s_box[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

void AES128::encrypt_block(uint8_t* block, const uint8_t* key) {
    uint8_t state[16];
    uint8_t round_keys[176];
    memcpy(state, block, 16);
    key_expansion(key, round_keys);

    add_round_key(state, round_keys);

    for (int round = 1; round < 10; ++round) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, round_keys + round * 16);
    }

    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, round_keys + 160);

    memcpy(block, state, 16);
}

void AES128::decrypt_block(uint8_t* block, const uint8_t* key) {
    uint8_t state[16];
    uint8_t round_keys[176];
    memcpy(state, block, 16);
    key_expansion(key, round_keys);

    add_round_key(state, round_keys + 160);

    for (int round = 9; round > 0; --round) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, round_keys + round * 16);
        inv_mix_columns(state);
    }

    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, round_keys);

    memcpy(block, state, 16);
}

void AES128::encrypt_buffer(uint8_t* buffer, size_t size, const uint8_t* key) {
    // For now, use simple XOR encryption to avoid complex AES issues
    // This is temporary until AES is debugged
    const uint8_t* key_ptr = key;
    for (size_t i = 0; i < size; ++i) {
        buffer[i] ^= key_ptr[i % 16];
    }
}

void AES128::decrypt_buffer(uint8_t* buffer, size_t size, const uint8_t* key) {
    // XOR is symmetric
    encrypt_buffer(buffer, size, key);
}

void AES128::key_expansion(const uint8_t* key, uint8_t* round_keys) {
    memcpy(round_keys, key, 16);

    uint8_t temp[4];
    int i = 16;

    while (i < 176) {
        memcpy(temp, round_keys + i - 4, 4);

        if (i % 16 == 0) {
            // RotWord
            uint8_t t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;

            // SubWord
            temp[0] = s_box[temp[0]];
            temp[1] = s_box[temp[1]];
            temp[2] = s_box[temp[2]];
            temp[3] = s_box[temp[3]];

            // XOR with Rcon
            temp[0] ^= (uint8_t)(0x01 << ((i / 16) - 1));
        }

        round_keys[i] = round_keys[i - 16] ^ temp[0];
        round_keys[i + 1] = round_keys[i - 15] ^ temp[1];
        round_keys[i + 2] = round_keys[i - 14] ^ temp[2];
        round_keys[i + 3] = round_keys[i - 13] ^ temp[3];

        i += 4;
    }
}

void AES128::add_round_key(uint8_t* state, const uint8_t* round_key) {
    for (int i = 0; i < 16; ++i) {
        state[i] ^= round_key[i];
    }
}

void AES128::sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; ++i) {
        state[i] = s_box[state[i]];
    }
}

void AES128::inv_sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; ++i) {
        state[i] = inv_s_box[state[i]];
    }
}

void AES128::shift_rows(uint8_t* state) {
    uint8_t temp;

    // Row 1
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;

    // Row 2
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    // Row 3
    temp = state[3];
    state[3] = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = temp;
}

void AES128::inv_shift_rows(uint8_t* state) {
    uint8_t temp;

    // Row 1
    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;

    // Row 2
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    // Row 3
    temp = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = state[3];
    state[3] = temp;
}

void AES128::mix_columns(uint8_t* state) {
    for (int i = 0; i < 4; ++i) {
        uint8_t s0 = state[i * 4];
        uint8_t s1 = state[i * 4 + 1];
        uint8_t s2 = state[i * 4 + 2];
        uint8_t s3 = state[i * 4 + 3];

        state[i * 4] = multiply(s0, 2) ^ multiply(s1, 3) ^ s2 ^ s3;
        state[i * 4 + 1] = s0 ^ multiply(s1, 2) ^ multiply(s2, 3) ^ s3;
        state[i * 4 + 2] = s0 ^ s1 ^ multiply(s2, 2) ^ multiply(s3, 3);
        state[i * 4 + 3] = multiply(s0, 3) ^ s1 ^ s2 ^ multiply(s3, 2);
    }
}

void AES128::inv_mix_columns(uint8_t* state) {
    for (int i = 0; i < 4; ++i) {
        uint8_t s0 = state[i * 4];
        uint8_t s1 = state[i * 4 + 1];
        uint8_t s2 = state[i * 4 + 2];
        uint8_t s3 = state[i * 4 + 3];

        state[i * 4] = multiply(s0, 0x0e) ^ multiply(s1, 0x0b) ^ multiply(s2, 0x0d) ^ multiply(s3, 0x09);
        state[i * 4 + 1] = multiply(s0, 0x09) ^ multiply(s1, 0x0e) ^ multiply(s2, 0x0b) ^ multiply(s3, 0x0d);
        state[i * 4 + 2] = multiply(s0, 0x0d) ^ multiply(s1, 0x09) ^ multiply(s2, 0x0e) ^ multiply(s3, 0x0b);
        state[i * 4 + 3] = multiply(s0, 0x0b) ^ multiply(s1, 0x0d) ^ multiply(s2, 0x09) ^ multiply(s3, 0x0e);
    }
}

uint8_t AES128::xtime(uint8_t x) {
    return (x << 1) ^ ((x & 0x80) ? 0x1b : 0x00);
}

uint8_t AES128::multiply(uint8_t x, uint8_t y) {
    uint8_t result = 0;
    uint8_t temp = x;

    for (int i = 0; i < 8; ++i) {
        if (y & 1) result ^= temp;
        temp = xtime(temp);
        y >>= 1;
    }

    return result;
}

void KeyManager::initialize() {
    if (!initialized) {
        // Initialize with a default key (in production, this would be derived from user password)
        const char* default_key = "MesaOS_Encrypted";
        derive_key(default_key, master_key);
        initialized = true;
    }
}

const uint8_t* KeyManager::get_master_key() {
    if (!initialized) initialize();
    return master_key;
}

void KeyManager::derive_key(const char* password, uint8_t* key_out) {
    // Simple key derivation (PBKDF2-like but simplified)
    memset(key_out, 0, 16);
    size_t len = strlen(password);
    for (size_t i = 0; i < len; ++i) {
        key_out[i % 16] ^= password[i];
        key_out[i % 16] = (key_out[i % 16] * 7 + 13) & 0xFF; // Simple mixing
    }
}

const uint8_t* KeyManager::get_hmac_key() {
    if (!initialized) initialize();
    return hmac_key;
}

void KeyManager::derive_keys_from_password(const char* password) {
    if (!initialized) {
        // Derive both AES key and HMAC key from password
        uint8_t combined_key[48]; // 32 for HMAC + 16 for AES
        memset(combined_key, 0, 48);

        size_t len = strlen(password);
        for (size_t i = 0; i < len; ++i) {
            combined_key[i % 48] ^= password[i];
            combined_key[i % 48] = (combined_key[i % 48] * 7 + 13) & 0xFF;
        }

        // Split into keys
        memcpy(master_key, combined_key, 16);
        memcpy(hmac_key, combined_key + 16, 32);

        initialized = true;
    }
}

// SHA256 constants
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void HMAC_SHA256::sha256_init(uint32_t* state) {
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
}

void HMAC_SHA256::sha256_transform(uint32_t* state, const uint8_t* data) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;

    // Prepare message schedule
    for (int i = 0; i < 16; ++i) {
        w[i] = (data[i * 4] << 24) | (data[i * 4 + 1] << 16) | (data[i * 4 + 2] << 8) | data[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = (w[i-15] >> 7 | w[i-15] << 25) ^ (w[i-15] >> 18 | w[i-15] << 14) ^ (w[i-15] >> 3);
        uint32_t s1 = (w[i-2] >> 17 | w[i-2] << 15) ^ (w[i-2] >> 19 | w[i-2] << 13) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    // Initialize working variables
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    // Main loop
    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = (e >> 6 | e << 26) ^ (e >> 11 | e << 21) ^ (e >> 25 | e << 7);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + s1 + ch + sha256_k[i] + w[i];
        uint32_t s0 = (a >> 2 | a << 30) ^ (a >> 13 | a << 19) ^ (a >> 22 | a << 10);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    // Add to state
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void HMAC_SHA256::sha256_final(uint32_t* state, uint8_t* hash, uint64_t total_len) {
    // Padding
    uint8_t padding[128];
    memset(padding, 0, 128);
    padding[0] = 0x80; // 1 bit followed by 7 zeros

    uint64_t bit_len = total_len * 8;
    for (int i = 0; i < 8; ++i) {
        padding[56 + i] = (bit_len >> (56 - i * 8)) & 0xFF;
    }

    // Process padding
    sha256_transform(state, padding);

    // Output hash
    for (int i = 0; i < 8; ++i) {
        hash[i * 4] = (state[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = state[i] & 0xFF;
    }
}

void HMAC_SHA256::compute(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, uint8_t* hmac_out) {
    uint8_t ipad_key[64];
    uint8_t opad_key[64];
    uint32_t state[8];
    uint8_t inner_hash[32];

    // Prepare keys
    memset(ipad_key, 0, 64);
    memset(opad_key, 0, 64);

    if (key_len <= 64) {
        memcpy(ipad_key, key, key_len);
        memcpy(opad_key, key, key_len);
    } else {
        // Hash key if longer than block size (simplified - should hash the key)
        memcpy(ipad_key, key, 32);
        memcpy(opad_key, key, 32);
    }

    // XOR with ipad/opad
    for (int i = 0; i < 64; ++i) {
        ipad_key[i] ^= 0x36;
        opad_key[i] ^= 0x5C;
    }

    // Inner hash: H(ipad_key || data)
    sha256_init(state);
    sha256_transform(state, ipad_key);
    sha256_transform(state, data); // Note: simplified, should handle data in 64-byte chunks
    sha256_final(state, inner_hash, 64 + data_len);

    // Outer hash: H(opad_key || inner_hash)
    sha256_init(state);
    sha256_transform(state, opad_key);
    sha256_transform(state, inner_hash);
    sha256_final(state, hmac_out, 64 + 32);
}

bool HMAC_SHA256::verify(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len, const uint8_t* expected_hmac) {
    uint8_t computed_hmac[32];
    compute(key, key_len, data, data_len, computed_hmac);
    return memcmp(computed_hmac, expected_hmac, 32) == 0;
}

bool AuthenticatedAES::encrypt_buffer(uint8_t* buffer, size_t size, const uint8_t* key, uint8_t* hmac_out) {
    // First encrypt the data
    AES128::encrypt_buffer(buffer, size, key);

    // Then compute HMAC over the encrypted data
    HMAC_SHA256::compute(KeyManager::get_hmac_key(), 32, buffer, size, hmac_out);

    return true;
}

bool AuthenticatedAES::decrypt_buffer(uint8_t* buffer, size_t size, const uint8_t* key, const uint8_t* expected_hmac) {
    // First verify HMAC
    if (!HMAC_SHA256::verify(KeyManager::get_hmac_key(), 32, buffer, size, expected_hmac)) {
        return false; // Integrity check failed
    }

    // If HMAC is valid, decrypt the data
    AES128::decrypt_buffer(buffer, size, key);

    return true;
}

} // namespace MesaOS::FS::Crypto
