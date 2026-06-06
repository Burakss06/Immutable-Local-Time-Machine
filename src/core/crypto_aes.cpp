#include "crypto_aes.h"
#include <cstring>
#include <iostream>
#include <iomanip>

// MasterKey ve BlockHash birleştirilerek benzersiz bir Blok Şifreleme Anahtarı türetilir
static Sha256Hash DeriveBlockKey(const Sha256Hash& masterKey, const Sha256Hash& blockHash) {
    std::array<uint8_t, 64> buffer;
    std::memcpy(buffer.data(), masterKey.data(), 32);
    std::memcpy(buffer.data() + 32, blockHash.data(), 32);
    
    Sha256Hash derivedKey;
    CalculateSHA256(buffer.data(), 64, derivedKey);
    return derivedKey;
}

bool EncryptBlockAES(const uint8_t* plaintext, size_t size, 
                     const Sha256Hash& blockHash, const Sha256Hash& masterKey, 
                     std::vector<uint8_t>& ciphertext) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;
    
    // 1. AES Algoritma Sağlayıcısını Aç
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: Algoritma saglayici acilamadi. Kod: 0x" << std::hex << status << std::dec << std::endl;
        return false;
    }
    
    // 2. Chaining modunu CBC olarak ayarla
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: CBC zincir modu ayarlanamadi. Kod: 0x" << std::hex << status << std::dec << std::endl;
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    // 3. Anahtar ve IV türet
    Sha256Hash blockKey = DeriveBlockKey(masterKey, blockHash);
    std::array<uint8_t, 16> iv;
    std::memcpy(iv.data(), blockHash.data(), 16); // IV olarak hash'in ilk 16 byte'ı
    
    // 4. BCrypt Key Nesnesi oluştur
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, blockKey.data(), 32, 0);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: Simetrik anahtar uretilemedi. Kod: 0x" << std::hex << status << std::dec << std::endl;
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    // 5. PKCS7 Padding ile gereken boyutu sorgula
    DWORD cipherSize = 0;
    status = BCryptEncrypt(hKey, const_cast<PUCHAR>(plaintext), static_cast<ULONG>(size), nullptr, 
                           iv.data(), 16, nullptr, 0, &cipherSize, BCRYPT_BLOCK_PADDING);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: Sifreleme boyutu sorgulanamadi. Kod: 0x" << std::hex << status << std::dec << std::endl;
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    ciphertext.resize(cipherSize);
    
    // 6. Şifreleme işlemini gerçekleştir
    DWORD bytesWritten = 0;
    std::memcpy(iv.data(), blockHash.data(), 16); // IV sıfırlanır
    status = BCryptEncrypt(hKey, const_cast<PUCHAR>(plaintext), static_cast<ULONG>(size), nullptr, 
                           iv.data(), 16, ciphertext.data(), cipherSize, &bytesWritten, BCRYPT_BLOCK_PADDING);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: Sifreleme basarisiz. Kod: 0x" << std::hex << status << std::dec << std::endl;
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return true;
}

bool DecryptBlockAES(const uint8_t* ciphertext, size_t size, 
                     const Sha256Hash& blockHash, const Sha256Hash& masterKey, 
                     std::vector<uint8_t>& plaintext) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;
    
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: Algoritma saglayici acilamadi. Kod: 0x" << std::hex << status << std::dec << std::endl;
        return false;
    }
    
    status = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: CBC zincir modu ayarlanamadi. Kod: 0x" << std::hex << status << std::dec << std::endl;
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    Sha256Hash blockKey = DeriveBlockKey(masterKey, blockHash);
    std::array<uint8_t, 16> iv;
    std::memcpy(iv.data(), blockHash.data(), 16);
    
    status = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, blockKey.data(), 32, 0);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: Simetrik anahtar uretilemedi. Kod: 0x" << std::hex << status << std::dec << std::endl;
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    // Gerekli düz metin boyutunu sorgula
    DWORD plainSize = 0;
    status = BCryptDecrypt(hKey, const_cast<PUCHAR>(ciphertext), static_cast<ULONG>(size), nullptr, 
                           iv.data(), 16, nullptr, 0, &plainSize, BCRYPT_BLOCK_PADDING);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: Desifreleme boyutu sorgulanamadi. Kod: 0x" << std::hex << status << " Size: " << size << std::dec << std::endl;
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    plaintext.resize(plainSize);
    
    DWORD bytesWritten = 0;
    std::memcpy(iv.data(), blockHash.data(), 16);
    status = BCryptDecrypt(hKey, const_cast<PUCHAR>(ciphertext), static_cast<ULONG>(size), nullptr, 
                           iv.data(), 16, plaintext.data(), plainSize, &bytesWritten, BCRYPT_BLOCK_PADDING);
    if (status != 0) {
        std::cerr << "[!] BCRYPT HATA: Desifreleme basarisiz. Kod: 0x" << std::hex << status << std::dec << std::endl;
        BCryptDestroyKey(hKey);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    
    plaintext.resize(bytesWritten); // Padding sonrası gerçek boyuta indir
    
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return true;
}
