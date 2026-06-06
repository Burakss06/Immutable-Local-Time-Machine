#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include "vault_storage.h"
#include "watcher.h"
#include "filter_engine.h"
#include "cloud_sync.h"

// IPC Protokol komutları
enum IpcCommand : uint32_t {
    CMD_START_WATCHER = 1,
    CMD_STOP_WATCHER = 2,
    CMD_GET_VERSIONS = 3,
    CMD_RESTORE = 4,
    CMD_GET_STATUS = 5,          // Durum sorgusu (0=Boşta, 1=İzliyor, 2=Kilitlendi/Ransomware)
    CMD_SET_RULES = 6,           // Glob filtre kuralları tanımlama
    CMD_GET_VERSION_CONTENT = 7,  // Bir sürümün ham içeriğini çözüp okuma
    CMD_RESPONSE = 100
};

#pragma pack(push, 1)

// 12 Byte boyutundaki IPC Üstbilgi (Header) Yapısı
struct IpcHeader {
    uint32_t magic;         // 4 Byte: Güvenlik imzası (Örn: 'IPCM')
    uint32_t command;       // 4 Byte: Çalıştırılacak komut (IpcCommand)
    uint32_t payload_size;  // 4 Byte: Peşinden gelen verinin boyutu
};

#pragma pack(pop)

// Ransomware tespiti için kayan pencere veri yapısı
struct FileWriteLog {
    uint64_t timestamp;
    double entropy;
};

class IpcServer {
public:
    IpcServer(VaultStorage& storage);
    ~IpcServer();

    // IPC sunucusunu arka planda asenkron başlatır
    bool Start();
    
    // Sunucuyu kapatır ve uykudan uyandırır (Dummy Wake-up)
    void Stop();

private:
    // İstemci kabul döngüsünü koşturan thread fonksiyonu
    void ListenLoop();
    
    // Her bir istemci bağlantısını işleyen fonksiyon
    void HandleClient(HANDLE hPipe);

    // Bir dosyanın Shannon Entropisini hesaplar
    double CalculateShannonEntropy(const std::wstring& filePath);

    VaultStorage& m_storage;
    HANDLE m_hPipe;
    std::thread m_listenThread;
    std::atomic<bool> m_running;
    
    // Gözcünün arka plandaki dinamik örneği ve izlenen dizin yolu
    std::unique_ptr<DirectoryWatcher> m_watcher;
    std::wstring m_watchedPath;

    // Eşzamanlı kasa erişimini korumak için mutex
    std::mutex m_storageMutex;

    // Ransomware dedektörü ve aktif kalkan durumları
    std::atomic<bool> m_panicState;
    std::wstring m_panicFilePath;
    std::vector<FileWriteLog> m_writeLogs;
    std::mutex m_logsMutex;

    // Gelişmiş Filtreleme ve Bulut Senkronizasyon modülleri
    FilterEngine m_filterEngine;
    CloudSyncBridge m_cloudSync;
};
