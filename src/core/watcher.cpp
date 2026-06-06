#include "watcher.h"
#include "win32_utils.h"
#include <iostream>
#include <fstream>

DirectoryWatcher::DirectoryWatcher(const std::wstring& directoryPath, WatcherCallback callback)
    : m_dirPath(directoryPath), m_hDir(INVALID_HANDLE_VALUE), m_running(false), m_callback(callback) {
    m_buffer.resize(64 * 1024);
}

DirectoryWatcher::~DirectoryWatcher() {
    Stop();
}

bool DirectoryWatcher::Start() {
    if (m_running) return true;

    m_hDir = CreateFileW(
        m_dirPath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );

    if (m_hDir == INVALID_HANDLE_VALUE) {
        return false;
    }

    m_running = true;
    m_watchThread = std::thread(&DirectoryWatcher::WatchLoop, this);
    return true;
}

void DirectoryWatcher::Stop() {
    if (!m_running) return;

    m_running = false;
    
    // MinGW'nin pthread tabanlı thread handle uyuşmazlığı nedeniyle CancelSynchronousIo bazen başarısız olur.
    // Bu kilidi kırmak için klasörün içine sahte bir dosya yazarak gözcüyü (ReadDirectoryChangesW) doğal yoldan uyandırıyoruz!
    std::wstring wakePath = m_dirPath + L"\\.iltm_wake";
    std::ofstream file(WStringToANSI(wakePath));
    file.close();

    if (m_watchThread.joinable()) {
        m_watchThread.join();
    }

    if (m_hDir != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDir);
        m_hDir = INVALID_HANDLE_VALUE;
    }

    // Sahte uyandırma dosyasını diskten temizle
    DeleteFileW(wakePath.c_str());
}

void DirectoryWatcher::WatchLoop() {
    DWORD bytesReturned = 0;
    
    while (m_running) {
        BOOL success = ReadDirectoryChangesW(
            m_hDir,
            m_buffer.data(),
            static_cast<DWORD>(m_buffer.size()),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            nullptr,
            nullptr
        );

        if (!success || bytesReturned == 0) {
            break;
        }

        size_t offset = 0;
        do {
            auto* notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(&m_buffer[offset]);
            std::wstring fileName(notify->FileName, notify->FileNameLength / sizeof(WCHAR));
            
            // Sahte uyandırma dosyasını ve onun tetiklediği olayları yoksay
            if (fileName != L".iltm_wake") {
                std::wstring fullPath = m_dirPath + L"\\" + fileName;

                FileEvent evType = FileEvent::Modified;
                switch (notify->Action) {
                    case FILE_ACTION_ADDED:             evType = FileEvent::Added; break;
                    case FILE_ACTION_REMOVED:           evType = FileEvent::Removed; break;
                    case FILE_ACTION_MODIFIED:          evType = FileEvent::Modified; break;
                    case FILE_ACTION_RENAMED_OLD_NAME:  evType = FileEvent::RenamedOldName; break;
                    case FILE_ACTION_RENAMED_NEW_NAME:  evType = FileEvent::RenamedNewName; break;
                }

                if (m_callback) {
                    m_callback(fullPath, evType);
                }
            }

            if (notify->NextEntryOffset == 0) break;
            offset += notify->NextEntryOffset;
        } while (true);
    }
}
