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

// Büyük bir dosyayı 4 MB'lık bloklara (chunk) böler ve her birinin hash'ini çıkarır
inline std::vector<BlockInfo> SliceFile(const std::wstring& filepath, size_t chunkSize = 4 * 1024 * 1024) {
    std::vector<BlockInfo> blocks;
    // Windows geniş dosya yollarını desteklemek için WStringToANSI kullanılır
    std::string ansiPath = WStringToANSI(filepath);
    std::ifstream file;
    
    // Editörün dosya kilidini bırakması için 5 kez (50ms arayla) deneme yapıyoruz
    for (int i = 0; i < 5; ++i) {
        file.open(ansiPath, std::ios::binary);
        if (file.is_open()) break;
        Sleep(50); // Win32 API Sleep
    }
    if (!file.is_open()) return blocks;

    std::vector<uint8_t> buffer(chunkSize);
    
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), chunkSize);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) break;

        BlockInfo block;
        block.size = static_cast<uint32_t>(bytesRead);
        
        // Bloğun parmak izini hesapla
        if (CalculateSHA256(buffer.data(), static_cast<size_t>(bytesRead), block.hash)) {
            blocks.push_back(block);
        }
    }
    
    return blocks;
}
