#pragma once
#include <vector>
#include <string>
#include <fstream>
#include "crypto.h"
#include "win32_utils.h"

// Bir veri bloğunun (Chunk) kimlik bilgileri
struct BlockInfo {
    Sha256Hash hash;  // SHA-256 hash değeri (Tekil kimlik)
    uint32_t size;    // Bloğun gerçek byte boyutu
};

// Büyük bir dosyayı İçerik Duyarlı Bölümleme (Content-Defined Chunking - CDC) ile dinamik bloklara böler
inline std::vector<BlockInfo> SliceFile(const std::wstring& filepath, size_t chunkSize = 4 * 1024 * 1024) {
    std::vector<BlockInfo> blocks;
    std::string ansiPath = WStringToANSI(filepath);
    std::ifstream file;
    
    // Editörün dosya kilidini bırakması için 5 kez (50ms arayla) deneme yapıyoruz
    for (int i = 0; i < 5; ++i) {
        file.open(ansiPath, std::ios::binary);
        if (file.is_open()) break;
        Sleep(50); // Win32 API Sleep
    }
    if (!file.is_open()) return blocks;

    // CDC Parametreleri: Min 16 KB, Max 256 KB, Hedef ortalama 64 KB
    const size_t minSize = 16 * 1024;
    const size_t maxSize = 256 * 1024;
    const uint32_t mask = 0xFFFF; // Ortalama 64 KB'ta bir eşleşir (2^16)
    const uint32_t targetVal = 0x7E1D;

    // Polynomial Rolling Hash (Rabin-Karp tarzı) penceresi (W = 48)
    const uint32_t p = 31;
    uint32_t p_power = 1;
    for (int i = 0; i < 48; ++i) {
        p_power *= p;
    }

    std::vector<uint8_t> window(48, 0);
    size_t windowIdx = 0;
    uint32_t rollingHash = 0;

    std::vector<uint8_t> currentChunk;
    currentChunk.reserve(maxSize);

    char ch;
    while (file.get(ch)) {
        uint8_t byteIn = static_cast<uint8_t>(ch);
        currentChunk.push_back(byteIn);

        // Pencereden çıkan ve giren byte'lara göre hash güncelleme
        uint8_t byteOut = window[windowIdx];
        window[windowIdx] = byteIn;
        windowIdx = (windowIdx + 1) % 48;

        rollingHash = rollingHash * p + byteIn - byteOut * p_power;

        bool shouldSplit = false;
        if (currentChunk.size() >= maxSize) {
            shouldSplit = true;
        } else if (currentChunk.size() >= minSize) {
            if ((rollingHash & mask) == targetVal) {
                shouldSplit = true;
            }
        }

        if (shouldSplit) {
            BlockInfo block;
            block.size = static_cast<uint32_t>(currentChunk.size());
            if (CalculateSHA256(currentChunk.data(), currentChunk.size(), block.hash)) {
                blocks.push_back(block);
            }
            currentChunk.clear();
            rollingHash = 0;
            window.assign(48, 0);
            windowIdx = 0;
        }
    }

    // Kalan son bloğu paketle
    if (!currentChunk.empty()) {
        BlockInfo block;
        block.size = static_cast<uint32_t>(currentChunk.size());
        if (CalculateSHA256(currentChunk.data(), currentChunk.size(), block.hash)) {
            blocks.push_back(block);
        }
    }
    
    return blocks;
}
