#include "ipc_server.h"
#include "win32_utils.h"
#include <iostream>
#include <vector>
#include <cstring>

// Byte modunda parçalı verileri hedef boyuta ulaşana kadar döngüyle okur
static bool ReadBytes(HANDLE hPipe, void* buffer, DWORD bytesToRead) {
    DWORD totalBytesRead = 0;
    uint8_t* ptr = reinterpret_cast<uint8_t*>(buffer);
    
    while (totalBytesRead < bytesToRead) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(hPipe, ptr + totalBytesRead, bytesToRead - totalBytesRead, &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            return false;
        }
        totalBytesRead += bytesRead;
    }
    return true;
}

// Parçalı verileri boru hattına tamamen yazana kadar döngüyle gönderir
static bool WriteBytes(HANDLE hPipe, const void* buffer, DWORD bytesToWrite) {
    DWORD totalBytesWritten = 0;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buffer);
    
    while (totalBytesWritten < bytesToWrite) {
        DWORD bytesWritten = 0;
        BOOL ok = WriteFile(hPipe, ptr + totalBytesWritten, bytesToWrite - totalBytesWritten, &bytesWritten, nullptr);
        if (!ok || bytesWritten == 0) {
            return false;
        }
        totalBytesWritten += bytesWritten;
    }
    return true;
}

static bool SendResponseStatus(HANDLE hPipe, uint32_t status) {
    IpcHeader header;
    header.magic = 0x4950434D; // 'IPCM'
    header.command = CMD_RESPONSE;
    header.payload_size = sizeof(status);
    
    if (!WriteBytes(hPipe, &header, sizeof(IpcHeader))) return false;
    return WriteBytes(hPipe, &status, sizeof(status));
}

IpcServer::IpcServer(VaultStorage& storage) 
    : m_storage(storage), m_hPipe(INVALID_HANDLE_VALUE), m_running(false) {}

IpcServer::~IpcServer() {
    Stop();
}

bool IpcServer::Start() {
    if (m_running) return true;
    m_running = true;
    m_listenThread = std::thread(&IpcServer::ListenLoop, this);
    return true;
}

void IpcServer::Stop() {
    if (!m_running) return;
    m_running = false;
    
    // ConnectNamedPipe bloklamasını çözmek için sahte (dummy) bağlantı kuruyoruz
    HANDLE hWake = CreateFileW(
        L"\\\\.\\pipe\\ILTM_Secure_Pipe", 
        GENERIC_WRITE, 0, nullptr, 
        OPEN_EXISTING, 0, nullptr
    );
    if (hWake != INVALID_HANDLE_VALUE) {
        CloseHandle(hWake);
    }
    
    if (m_listenThread.joinable()) {
        m_listenThread.join();
    }
    
    if (m_watcher) {
        m_watcher->Stop();
        m_watcher.reset();
    }
}

void IpcServer::ListenLoop() {
    while (m_running) {
        // PIPE_TYPE_BYTE | PIPE_READMODE_BYTE ile ham byte-stream modunda boru açıyoruz
        m_hPipe = CreateNamedPipeW(
            L"\\\\.\\pipe\\ILTM_Secure_Pipe",
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, // Eşzamanlı 1 bağlantı (GUI Kontrolü)
            4096, 4096, 0, nullptr
        );
        
        if (m_hPipe == INVALID_HANDLE_VALUE) {
            Sleep(100);
            continue;
        }
        
        // İstemci bağlantısını bekler (Bloklanır)
        BOOL connected = ConnectNamedPipe(m_hPipe, nullptr) ? 
                         TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        
        if (!m_running) {
            CloseHandle(m_hPipe);
            break;
        }
        
        if (connected) {
            HandleClient(m_hPipe);
        }
        
        DisconnectNamedPipe(m_hPipe);
        CloseHandle(m_hPipe);
    }
}

void IpcServer::HandleClient(HANDLE hPipe) {
    std::wcout << L"\n[IPC] Yeni baglanti algilandi. Istek paketi okunuyor..." << std::endl;
    
    IpcHeader header;
    if (!ReadBytes(hPipe, &header, sizeof(IpcHeader))) {
        std::wcerr << L"[IPC-HATA] Istek basligi (Header) okunamadi! GetLastError: " << GetLastError() << std::endl;
        return;
    }
    
    std::wcout << L"[IPC] Baslik OK -> Magic: 0x" << std::hex << header.magic 
              << L", Komut ID: " << std::dec << header.command 
              << L", Payload Boyutu: " << header.payload_size << std::endl;
    
    // Magic doğrulaması
    if (header.magic != 0x4950434D) {
        std::wcerr << L"[IPC-HATA] Gecersiz Magic imzasi!" << std::endl;
        SendResponseStatus(hPipe, 0); // Hatalı Protokol
        return;
    }
    
    std::vector<uint8_t> payload(header.payload_size);
    if (header.payload_size > 0) {
        if (!ReadBytes(hPipe, payload.data(), header.payload_size)) {
            std::wcerr << L"[IPC-HATA] Payload okunamadi! GetLastError: " << GetLastError() << std::endl;
            return;
        }
    }
    
    switch (header.command) {
        case CMD_START_WATCHER: {
            if (payload.size() < 4) {
                std::wcerr << L"[IPC-HATA] CMD_START_WATCHER eksik parametre!" << std::endl;
                SendResponseStatus(hPipe, 0);
                break;
            }
            uint32_t charCount = 0;
            std::memcpy(&charCount, payload.data(), 4);
            if (payload.size() < 4 + charCount * sizeof(wchar_t)) {
                std::wcerr << L"[IPC-HATA] CMD_START_WATCHER dizin boyutu tutarsiz!" << std::endl;
                SendResponseStatus(hPipe, 0);
                break;
            }
            
            std::wstring watchPath(reinterpret_cast<const wchar_t*>(payload.data() + 4), charCount);
            std::wcout << L"[IPC] Gozcu baslatiliyor. Hedef Dizin: " << watchPath << std::endl;
            
            std::lock_guard<std::mutex> lock(m_storageMutex);
            if (m_watcher) {
                m_watcher->Stop();
                m_watcher.reset();
            }
            
            m_watchedPath = watchPath;
            m_watcher = std::make_unique<DirectoryWatcher>(m_watchedPath, [this](const std::wstring& filePath, FileEvent event) {
                if (event == FileEvent::Added || event == FileEvent::Modified) {
                    std::lock_guard<std::mutex> innerLock(m_storageMutex);
                    m_storage.BackupFile(filePath);
                }
            });
            
            bool ok = m_watcher->Start();
            std::wcout << L"[IPC] Gozcu baslatildi. Durum: " << (ok ? L"BASARILI" : L"HATA") << std::endl;
            SendResponseStatus(hPipe, ok ? 1 : 0);
            break;
        }
        
        case CMD_STOP_WATCHER: {
            std::wcout << L"[IPC] Gozcu durduruluyor..." << std::endl;
            std::lock_guard<std::mutex> lock(m_storageMutex);
            if (m_watcher) {
                m_watcher->Stop();
                m_watcher.reset();
                m_watchedPath.clear();
            }
            std::wcout << L"[IPC] Gozcu durduruldu." << std::endl;
            SendResponseStatus(hPipe, 1);
            break;
        }
        
        case CMD_GET_VERSIONS: {
            if (payload.size() < 4) {
                std::wcerr << L"[IPC-HATA] CMD_GET_VERSIONS eksik parametre!" << std::endl;
                SendResponseStatus(hPipe, 0);
                break;
            }
            uint32_t pathLen = 0;
            std::memcpy(&pathLen, payload.data(), 4);
            if (payload.size() < 4 + pathLen * sizeof(wchar_t)) {
                std::wcerr << L"[IPC-HATA] CMD_GET_VERSIONS yol boyutu tutarsiz!" << std::endl;
                SendResponseStatus(hPipe, 0);
                break;
            }
            
            std::wstring originPath(reinterpret_cast<const wchar_t*>(payload.data() + 4), pathLen);
            std::wcout << L"[IPC] Surum sorgusu alindi. Dosya: " << originPath << std::endl;
            
            std::lock_guard<std::mutex> lock(m_storageMutex);
            const auto* history = m_storage.GetFileHistory(originPath);
            
            std::vector<uint8_t> respPayload;
            uint32_t count = (history != nullptr) ? static_cast<uint32_t>(history->size()) : 0;
            
            respPayload.resize(4);
            std::memcpy(respPayload.data(), &count, 4);
            
            if (count > 0) {
                for (const auto& ver : *history) {
                    uint64_t timestamp = ver.timestamp;
                    uint32_t blockCount = static_cast<uint32_t>(ver.blockHashes.size());
                    
                    size_t currSize = respPayload.size();
                    respPayload.resize(currSize + sizeof(uint64_t) + sizeof(uint32_t));
                    std::memcpy(respPayload.data() + currSize, &timestamp, sizeof(uint64_t));
                    std::memcpy(respPayload.data() + currSize + sizeof(uint64_t), &blockCount, sizeof(uint32_t));
                }
            }
            
            IpcHeader respHeader;
            respHeader.magic = 0x4950434D;
            respHeader.command = CMD_RESPONSE;
            respHeader.payload_size = static_cast<uint32_t>(respPayload.size());
            
            if (WriteBytes(hPipe, &respHeader, sizeof(IpcHeader)) && 
                WriteBytes(hPipe, respPayload.data(), static_cast<DWORD>(respPayload.size()))) {
                std::wcout << L"[IPC] Surum listesi yaniti gonderildi. Bulunan surum: " << count << std::endl;
            } else {
                std::wcerr << L"[IPC-HATA] Surum listesi yaniti gonderilemedi! GetLastError: " << GetLastError() << std::endl;
            }
            break;
        }
        
        case CMD_RESTORE: {
            if (payload.size() < 12) { // index(4) + origin_len(4) + dest_len(4)
                std::wcerr << L"[IPC-HATA] CMD_RESTORE eksik parametre!" << std::endl;
                SendResponseStatus(hPipe, 0);
                break;
            }
            uint32_t versionIndex = 0;
            uint32_t originLen = 0;
            std::memcpy(&versionIndex, payload.data(), 4);
            std::memcpy(&originLen, payload.data() + 4, 4);
            
            if (payload.size() < 12 + originLen * sizeof(wchar_t)) {
                std::wcerr << L"[IPC-HATA] CMD_RESTORE kaynak yol boyutu tutarsiz!" << std::endl;
                SendResponseStatus(hPipe, 0);
                break;
            }
            std::wstring originPath(reinterpret_cast<const wchar_t*>(payload.data() + 8), originLen);
            
            uint32_t destLen = 0;
            size_t destLenOffset = 8 + originLen * sizeof(wchar_t);
            std::memcpy(&destLen, payload.data() + destLenOffset, 4);
            
            if (payload.size() < destLenOffset + 4 + destLen * sizeof(wchar_t)) {
                std::wcerr << L"[IPC-HATA] CMD_RESTORE hedef yol boyutu tutarsiz!" << std::endl;
                SendResponseStatus(hPipe, 0);
                break;
            }
            std::wstring destPath(reinterpret_cast<const wchar_t*>(payload.data() + destLenOffset + 4), destLen);
            
            std::wcout << L"[IPC] Geri yukleme baslatiliyor. Dosya: " << originPath 
                      << L" -> " << destPath << L" (Surum Indisi: " << versionIndex << L")" << std::endl;
            
            std::lock_guard<std::mutex> lock(m_storageMutex);
            bool success = m_storage.RestoreFile(originPath, versionIndex, destPath);
            std::wcout << L"[IPC] Geri yukleme bitti. Sonuc: " << (success ? L"BASARILI" : L"HATA") << std::endl;
            SendResponseStatus(hPipe, success ? 1 : 0);
            break;
        }
        
        default:
            std::wcerr << L"[IPC-HATA] Bilinmeyen Komut ID: " << header.command << std::endl;
            SendResponseStatus(hPipe, 0); // Bilinmeyen Komut
            break;
    }
}
