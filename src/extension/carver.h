#pragma once
#include <vector>
#include <string>

// Dosya kazıma (Carving) için imza tanımı
struct FileSignature {
    std::string ext;              // Dosya uzantısı (Örn: "png")
    std::vector<uint8_t> header;  // Başlangıç imzası (Header Magic)
    std::vector<uint8_t> footer;  // Bitiş imzası (Footer Magic)
};

// Kurtarılan dosya bilgisi
struct CarvedFile {
    std::string ext;
    uint64_t startOffset;
    std::vector<uint8_t> data;
};

// Bellekteki ham disk görüntüsü üzerinden dosya kazıma (Carving) yapar
std::vector<CarvedFile> CarveData(const std::vector<uint8_t>& rawData, const FileSignature& sig);
