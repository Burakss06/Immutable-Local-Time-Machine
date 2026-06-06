#pragma once
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 // Windows Vista ve üzerini hedefleyerek CancelSynchronousIo'yu aktif eder
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

// Klasör izleme olay tipleri
enum class FileEvent {
    Added,
    Removed,
    Modified,
    RenamedOldName,
    RenamedNewName
};

// Gözcünün olay bildireceği geri çağırım (Callback) imzası
using WatcherCallback = std::function<void(const std::wstring& filePath, FileEvent eventType)>;

class DirectoryWatcher {
public:
    DirectoryWatcher(const std::wstring& directoryPath, WatcherCallback callback);
    ~DirectoryWatcher();

    bool Start();
    void Stop();

private:
    void WatchLoop();

    std::wstring m_dirPath;
    HANDLE m_hDir;
    std::thread m_watchThread;
    std::atomic<bool> m_running;
    std::vector<uint8_t> m_buffer;
    WatcherCallback m_callback; // Olaylar tetiklendiğinde çağrılacak fonksiyon
};
