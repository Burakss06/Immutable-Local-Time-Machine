#pragma once
#include <string>
#include <fstream>
#include "vault_format.h"
#include "win32_utils.h"

// VaultHeader yapısını diske yazar
inline bool SaveHeader(const std::wstring& filepath, const VaultHeader& header) {
    std::ofstream file(WStringToANSI(filepath), std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(&header), sizeof(VaultHeader));
    return file.good();
}

// Bir kasanın geçerli olup olmadığını doğrular
inline bool ValidateVaultHeader(const VaultHeader& header, uint64_t fileSize) {
    if (header.magic[0] != 'I' || header.magic[1] != 'L' ||
        header.magic[2] != 'T' || header.magic[3] != 'M') {
        return false;
    }
    if (header.version != 1) {
        return false;
    }
    if (header.index_offset < sizeof(VaultHeader) || header.index_offset > fileSize) {
        return false;
    }
    return true;
}

// VaultHeader yapısını diskten güvenli şekilde okur ve doğrular
inline bool LoadAndValidateHeader(const std::wstring& filepath, VaultHeader& header) {
    if (!Win32FileExists(filepath)) return false;
    
    uint64_t fileSize = Win32GetFileSize(filepath);
    if (fileSize < sizeof(VaultHeader)) return false;
    
    std::ifstream file(WStringToANSI(filepath), std::ios::binary);
    if (!file.is_open()) return false;
    
    file.read(reinterpret_cast<char*>(&header), sizeof(VaultHeader));
    if (!file.good()) return false;
    
    return ValidateVaultHeader(header, fileSize);
}
