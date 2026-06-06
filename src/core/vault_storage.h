#pragma once
#include <string>
#include <vector>
#include "vault_format.h"
#include "vault_io.h"
#include "index.h"
#include "dedup.h"

class VaultStorage {
public:
    VaultStorage(const std::wstring& vaultFilePath);
    ~VaultStorage();

    // Kasayı açar, başlığı okur ve indeksi yükler
    bool Initialize(const Sha256Hash& masterKey);

    // Bir dosyayı dilimleyip tekilleştirerek kasaya yedekler
    bool BackupFile(const std::wstring& filePath);

    // Bir dosyanın sürüm geçmişini döner
    const std::vector<FileVersion>* GetFileHistory(const std::wstring& filePath) const {
        return m_index.GetFileHistory(filePath);
    }

    const std::wstring& GetVaultPath() const { return m_vaultPath; }

    // Bir dosyanın belirli bir sürümünü dışarıya geri yükler (Restore)
    bool RestoreFile(const std::wstring& filePath, size_t versionIndex, const std::wstring& destPath);

    // Bir dosyanın belirli bir sürümünün ham deşifre edilmiş içeriğini belleğe okur
    bool GetVersionContent(const std::wstring& filePath, size_t versionIndex, std::vector<uint8_t>& outContent);

    // RAM'deki indeksi kasanın sonuna yazar ve dosya başlığını günceller
    bool CommitIndex();

private:
    std::wstring m_vaultPath;
    VaultHeader m_header;
    VaultIndex m_index;
    Sha256Hash m_masterKey; // Blok şifrelemesinde kullanılacak anahtar
    
    // Yardımcı fonksiyonlar: İndeksi serileştirme / deserileştirme
    bool WriteIndexToStream(std::ofstream& out);
    bool ReadIndexFromStream(std::ifstream& in);
};
