#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <array>
#include "crypto.h"
#include "radix_tree.h"

// Sha256Hash (std::array<uint8_t, 32>) yapısını unordered_map içinde anahtar (Key) olarak 
// kullanabilmek için özel bir hash fonksiyonu (Hasher) tanımlamalıyız.
struct Sha256HashHasher {
    size_t operator()(const Sha256Hash& hash) const {
        // Hash'in ilk 8 byte'ını size_t olarak kopyalayarak hızlıca hash üretiyoruz
        size_t result = 0;
        std::memcpy(&result, hash.data(), sizeof(size_t));
        return result;
    }
};

// Kasadaki blokların ve dosyaların RAM üzerindeki indeks motoru
class VaultIndex {
    friend class VaultStorage; // Depolama katmanına ham verilere erişim izni verir
public:
    // Benzersiz bir veri bloğunun kasadaki offset adresini kaydeder
    void RegisterChunk(const Sha256Hash& hash, uint64_t offset) {
        m_chunkMap[hash] = offset;
    }

    // Bir bloğun kasada kayıtlı olup olmadığını kontrol eder, varsa offset'i döner
    bool GetChunkOffset(const Sha256Hash& hash, uint64_t& out_offset) const {
        auto it = m_chunkMap.find(hash);
        if (it == m_chunkMap.end()) return false;
        out_offset = it->second;
        return true;
    }

    // Bir dosya sürümünü indekse ekler
    void AddFileVersion(const std::wstring& filePath, const FileVersion& version) {
        m_fileMap.Insert(filePath, version);
    }

    // Bir dosyanın sürüm geçmişini döner
    const std::vector<FileVersion>* GetFileHistory(const std::wstring& filePath) const {
        return m_fileMap.Lookup(filePath);
    }

private:
    // Hash -> Kasa dosyasındaki offset (Tekil bloklar için)
    std::unordered_map<Sha256Hash, uint64_t, Sha256HashHasher> m_chunkMap;

    // Dosya Yolu -> Sürüm Geçmişi (Dinamik önek sıkıştırmalı Radix Tree)
    RadixTree m_fileMap;
};
