#pragma once
#include <vector>
#include <array>
#include <windows.h>
#include <bcrypt.h>
#include "crypto.h"

// AES-256 için Blok Boyutu (16 Byte) ve Anahtar Boyutu (32 Byte)
constexpr size_t AES_BLOCK_SIZE = 16;
constexpr size_t AES_KEY_SIZE = 32;

// Katılımsal şifreleme (Convergent Encryption) kullanarak bloğu şifreler
bool EncryptBlockAES(const uint8_t* plaintext, size_t size, 
                     const Sha256Hash& blockHash, const Sha256Hash& masterKey, 
                     std::vector<uint8_t>& ciphertext);

// Şifrelenmiş bloğu geri çözer
bool DecryptBlockAES(const uint8_t* ciphertext, size_t size, 
                     const Sha256Hash& blockHash, const Sha256Hash& masterKey, 
                     std::vector<uint8_t>& plaintext);
