#pragma once
#include <cstdint>

#pragma pack(push, 1)

// ILTM Özel İkili Kasa (Binary Vault) Üstbilgi Yapısı
struct VaultHeader {
    char magic[4];          // 4 Byte: Dosya imtiyaz imzası (Örn: 'ILTM')
    uint32_t version;       // 4 Byte: Kasa formatı versiyonu (Örn: 1)
    uint64_t index_offset;  // 8 Byte: İndeks tablosunun dosyadaki başlangıç yeri
    uint64_t total_chunks;  // 8 Byte: Dosyadaki toplam blok (chunk) sayısı
};

// Her bir veri bloğunun önünde yer alacak metadata yapısı
struct ChunkHeader {
    uint8_t hash[32];       // 32 Byte: SHA-256 Hash değeri
    uint32_t data_size;     // 4 Byte: Bloğun gerçek boyutu (Maksimum 4MB)
};

#pragma pack(pop)
