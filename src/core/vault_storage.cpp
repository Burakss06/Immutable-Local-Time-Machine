#include "vault_storage.h"
#include <chrono>
#include <iostream>
#include "win32_utils.h"

VaultStorage::VaultStorage(const std::wstring& vaultFilePath)
    : m_vaultPath(vaultFilePath) {
    m_header = { {'I', 'L', 'T', 'M'}, 1, sizeof(VaultHeader), 0 };
}

VaultStorage::~VaultStorage() {
    CommitIndex();
}

bool VaultStorage::Initialize() {
    // Kasa dosyası varsa oku ve doğrula, yoksa yeni oluştur
    if (Win32FileExists(m_vaultPath)) {
        if (!LoadAndValidateHeader(m_vaultPath, m_header)) {
            return false;
        }
        
        std::ifstream file(WStringToANSI(m_vaultPath), std::ios::binary);
        if (!file) return false;
        
        // İndeks tablosunun başlangıcına git ve oku
        file.seekg(m_header.index_offset);
        return ReadIndexFromStream(file);
    } else {
        // Yeni kasa dosyası oluştur ve başlığı yaz
        return SaveHeader(m_vaultPath, m_header);
    }
    return false;
}

bool VaultStorage::BackupFile(const std::wstring& filePath) {
    // 1. Dosyayı 4 MB'lık dilimlere ayır ve hash'lerini çıkar
    std::vector<BlockInfo> blocks = SliceFile(filePath);
    if (blocks.empty()) return false;

    // Kasa dosyasını okuma-yazma modunda aç
    std::fstream vault(WStringToANSI(m_vaultPath), std::ios::in | std::ios::out | std::ios::binary);
    if (!vault) return false;

    std::vector<Sha256Hash> fileBlockHashes;
    
    for (const auto& block : blocks) {
        uint64_t chunkOffset = 0;
        fileBlockHashes.push_back(block.hash);
        
        // Bu blok kasada zaten var mı? (Tekilleştirme kontrolü)
        if (m_index.GetChunkOffset(block.hash, chunkOffset)) {
            continue; // Varsa yazma, referansı kullan
        }

        // Yoksa: Bloğu kasaya ekle (Eski indeks tablosunun üzerine yazarız)
        vault.seekp(m_header.index_offset);
        chunkOffset = m_header.index_offset;

        // ChunkHeader yaz
        ChunkHeader chunkHeader;
        std::memcpy(chunkHeader.hash, block.hash.data(), 32);
        chunkHeader.data_size = block.size;
        vault.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(ChunkHeader));

        // Gerçek veri bloğunu oku ve kasaya yaz
        std::ifstream srcFile(WStringToANSI(filePath), std::ios::binary);
        if (!srcFile) return false;
        srcFile.seekg(fileBlockHashes.size() == 1 ? 0 : (fileBlockHashes.size() - 1) * 4 * 1024 * 1024);
        
        std::vector<uint8_t> buffer(block.size);
        srcFile.read(reinterpret_cast<char*>(buffer.data()), block.size);
        vault.write(reinterpret_cast<const char*>(buffer.data()), block.size);

        // İndeksi güncelle
        m_index.RegisterChunk(block.hash, chunkOffset);
        m_header.total_chunks++;

        // Bir sonraki bloğun yazılacağı yeri güncelle (index_offset'i kaydırıyoruz)
        m_header.index_offset = vault.tellp();
    }

    // Dosya için yeni bir sürüm kaydı oluştur
    FileVersion version;
    version.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    version.blockHashes = fileBlockHashes;

    m_index.AddFileVersion(filePath, version);
    return CommitIndex();
}

bool VaultStorage::CommitIndex() {
    std::ofstream file(WStringToANSI(m_vaultPath), std::ios::in | std::ios::out | std::ios::binary);
    if (!file) return false;

    // Kasanın en sonuna git (yeni indeks buraya yazılacak)
    file.seekp(m_header.index_offset);

    // İndeksi buraya yaz
    if (!WriteIndexToStream(file)) return false;

    // Header'ı güncelle
    file.seekp(0);
    file.write(reinterpret_cast<const char*>(&m_header), sizeof(VaultHeader));
    return file.good();
}

bool VaultStorage::WriteIndexToStream(std::ofstream& out) {
    // 1. Chunk haritasını yaz (Kaç adet benzersiz chunk var?)
    uint64_t chunkCount = m_index.m_chunkMap.size();
    out.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));

    for (const auto& [hash, offset] : m_index.m_chunkMap) {
        out.write(reinterpret_cast<const char*>(hash.data()), 32);
        out.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }

    // 2. Dosya geçmişi haritasını yaz (Kaç dosya var?)
    uint64_t fileCount = m_index.m_fileMap.size();
    out.write(reinterpret_cast<const char*>(&fileCount), sizeof(fileCount));

    for (const auto& [path, versions] : m_index.m_fileMap) {
        // Dosya yolunun uzunluğunu yaz (karakter sayısı)
        uint32_t pathLen = static_cast<uint32_t>(path.size());
        out.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        // Dosya yolunun kendisini (wchar_t dizisi) yaz
        out.write(reinterpret_cast<const char*>(path.data()), pathLen * sizeof(wchar_t));

        // Bu dosyanın kaç adet sürümü var?
        uint32_t versionCount = static_cast<uint32_t>(versions.size());
        out.write(reinterpret_cast<const char*>(&versionCount), sizeof(versionCount));

        for (const auto& ver : versions) {
            // Zaman damgasını yaz
            out.write(reinterpret_cast<const char*>(&ver.timestamp), sizeof(ver.timestamp));
            // Sürümü oluşturan blok sayısını yaz
            uint32_t blockCount = static_cast<uint32_t>(ver.blockHashes.size());
            out.write(reinterpret_cast<const char*>(&blockCount), sizeof(blockCount));

            for (const auto& h : ver.blockHashes) {
                out.write(reinterpret_cast<const char*>(h.data()), 32);
            }
        }
    }
    return out.good();
}

bool VaultStorage::ReadIndexFromStream(std::ifstream& in) {
    // 1. Chunk haritasını oku
    uint64_t chunkCount = 0;
    in.read(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));
    if (!in) return false;

    for (uint64_t i = 0; i < chunkCount; ++i) {
        Sha256Hash hash;
        uint64_t offset = 0;
        in.read(reinterpret_cast<char*>(hash.data()), 32);
        in.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        m_index.RegisterChunk(hash, offset);
    }

    // 2. Dosya geçmişi haritasını oku
    uint64_t fileCount = 0;
    in.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    if (!in) return false;

    for (uint64_t i = 0; i < fileCount; ++i) {
        uint32_t pathLen = 0;
        in.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
        std::wstring path(pathLen, L'\0');
        in.read(reinterpret_cast<char*>(path.data()), pathLen * sizeof(wchar_t));

        uint32_t versionCount = 0;
        in.read(reinterpret_cast<char*>(&versionCount), sizeof(versionCount));

        for (uint32_t v = 0; v < versionCount; ++v) {
            FileVersion ver;
            in.read(reinterpret_cast<char*>(&ver.timestamp), sizeof(ver.timestamp));
            uint32_t blockCount = 0;
            in.read(reinterpret_cast<char*>(&blockCount), sizeof(blockCount));

            for (uint32_t b = 0; b < blockCount; ++b) {
                Sha256Hash h;
                in.read(reinterpret_cast<char*>(h.data()), 32);
                ver.blockHashes.push_back(h);
            }
            m_index.AddFileVersion(path, ver);
        }
    }
    return !in.fail();
}

bool VaultStorage::RestoreFile(const std::wstring& filePath, size_t versionIndex, const std::wstring& destPath) {
    const auto* history = m_index.GetFileHistory(filePath);
    if (!history || versionIndex >= history->size()) return false;

    const auto& version = (*history)[versionIndex];

    // Geri yükleme dosyasını yazma modunda aç
    std::ofstream destFile(WStringToANSI(destPath), std::ios::binary);
    if (!destFile) return false;

    std::ifstream vault(WStringToANSI(m_vaultPath), std::ios::binary);
    if (!vault) return false;

    for (const auto& hash : version.blockHashes) {
        uint64_t offset = 0;
        if (!m_index.GetChunkOffset(hash, offset)) return false;

        // ChunkHeader'ı oku (boyut bilgisini doğrulamak için)
        vault.seekg(offset);
        ChunkHeader chunkHeader;
        vault.read(reinterpret_cast<char*>(&chunkHeader), sizeof(ChunkHeader));

        // Gerçek blok verisini oku
        std::vector<uint8_t> buffer(chunkHeader.data_size);
        vault.read(reinterpret_cast<char*>(buffer.data()), chunkHeader.data_size);

        // Geri yüklenen dosyaya yaz
        destFile.write(reinterpret_cast<const char*>(buffer.data()), chunkHeader.data_size);
    }
    return destFile.good();
}
