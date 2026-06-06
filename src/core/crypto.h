#pragma once
#include <vector>
#include <array>
#include <windows.h>
#include <bcrypt.h>

// BCrypt API'sini kullanabilmek için bcrypt.lib kütüphanesini bağlamamız gerekir.
#pragma comment(lib, "bcrypt.lib")

// SHA-256 Hash boyutu her zaman 32 byte'tır.
using Sha256Hash = std::array<uint8_t, 32>;

// Verilen veri tamponunun (buffer) SHA-256 parmak izini hesaplar
inline bool CalculateSHA256(const uint8_t* data, size_t size, Sha256Hash& out_hash) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    
    // 1. SHA-256 Sağlayıcısını Aç
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return false;
    }
    
    // 2. Hash Nesnesi Oluştur
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    // 3. Veriyi Hash İşlemine Sok
    if (BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0) != 0) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    // 4. Hash Değerini Tamamla ve Sonucu Al
    if (BCryptFinishHash(hHash, out_hash.data(), static_cast<ULONG>(out_hash.size()), 0) != 0) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    // 5. Kaynakları Temizle
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return true;
}
