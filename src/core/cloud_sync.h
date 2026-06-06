#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

class CloudSyncBridge {
public:
    CloudSyncBridge();
    ~CloudSyncBridge();

    // Yapılandırmayı diskten yükler
    void LoadConfig();
    
    // Yapılandırmayı kaydeder (mode: 0 = Devredışı, 1 = Yerel Ayna, 2 = HTTPS S3)
    void SaveConfig(int mode, const std::wstring& target);

    // Yedekleme bittiğinde bulut senkronizasyonunu asenkron olarak tetikler
    void TriggerSync(const std::wstring& vaultPath);

    int GetMode() const { return m_syncMode; }
    std::wstring GetTarget() const { return m_syncTarget; }

private:
    // Senkronizasyon işlemini arka plan thread'inde yürüten fonksiyon
    void WorkerThread(std::wstring vaultPath);

    // Yerel disk veya ağ paylaşımına yedekleme kopyalama
    bool SyncToLocalMirror(const std::wstring& srcPath, const std::wstring& destPath);

    // WinINet ile HTTPS PUT/POST yüklemesi
    bool SyncToHttpCloud(const std::wstring& srcPath, const std::wstring& urlStr);

    int m_syncMode; // 0 = None, 1 = Local Mirror, 2 = HTTP Upload
    std::wstring m_syncTarget; // Klasör yolu veya HTTPS S3/Presigned URL adresi
    std::mutex m_configMutex;
    std::atomic<bool> m_syncInProgress;
};
