#include "ipc_server.h"
#include "win32_utils.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <array>
#include <chrono>
#include <algorithm>

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
    : m_storage(storage), m_hPipe(INVALID_HANDLE_VALUE), m_running(false), m_panicState(false) {}

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
        // Tüm kullanıcılara (GUI vb.) erişim izni vermek için Null DACL hazırlıyoruz
        SECURITY_DESCRIPTOR sd;
        InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);
        
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle = FALSE;

        // PIPE_TYPE_BYTE | PIPE_READMODE_BYTE ile ham byte-stream modunda boru açıyoruz
        m_hPipe = CreateNamedPipeW(
            L"\\\\.\\pipe\\ILTM_Secure_Pipe",
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, // Eşzamanlı 1 bağlantı (GUI Kontrolü)
            4096, 4096, 0, &sa
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
            std::wcout << L"\n[IPC-DEBUG] ListenLoop: Client connected! Launching HandleClient..." << std::endl;
            HandleClient(m_hPipe);
            std::wcout << L"[IPC-DEBUG] ListenLoop: Flushing buffers..." << std::endl;
            FlushFileBuffers(m_hPipe);
        }
        
        std::wcout << L"[IPC-DEBUG] ListenLoop: Disconnecting named pipe..." << std::endl;
        DisconnectNamedPipe(m_hPipe);
        CloseHandle(m_hPipe);
    }
}

void IpcServer::HandleClient(HANDLE hPipe) {
    std::wcout << L"\n[IPC] Yeni baglanti algilandi. Istekler isleniyor..." << std::endl;
    std::wcout << L"[IPC-DEBUG] HandleClient: Istemci baglandi, veri bekleniyor..." << std::endl;
    
    while (m_running) {
        IpcHeader header;
        if (!ReadBytes(hPipe, &header, sizeof(IpcHeader))) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA || err == 0) {
                std::wcout << L"[IPC-DEBUG] HandleClient: Istemci baglantiyi kapatti (Broken Pipe/EOF)." << std::endl;
            } else {
                std::wcerr << L"[IPC-HATA] Istek basligi (Header) okunamadi! GetLastError: " << err << std::endl;
            }
            break;
        }
        
        std::wcout << L"[IPC] Baslik OK -> Magic: 0x" << std::hex << header.magic 
                  << L", Komut ID: " << std::dec << header.command 
                  << L", Payload Boyutu: " << header.payload_size << std::endl;
        
        // Magic doğrulaması
        if (header.magic != 0x4950434D) {
            std::wcerr << L"[IPC-HATA] Gecersiz Magic imzasi!" << std::endl;
            SendResponseStatus(hPipe, 0); // Hatalı Protokol
            break;
        }
        
        std::vector<uint8_t> payload(header.payload_size);
        if (header.payload_size > 0) {
            if (!ReadBytes(hPipe, payload.data(), header.payload_size)) {
                std::wcerr << L"[IPC-HATA] Payload okunamadi! GetLastError: " << GetLastError() << std::endl;
                std::wcout << L"[IPC-DEBUG] HandleClient: Istemci hatti kesti (Payload okunamadi)." << std::endl;
                break;
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
                    if (m_panicState) return;

                    if (event == FileEvent::Added || event == FileEvent::Modified) {
                        // 1. Akıllı Filtreleme Motoru Kontrolü
                        if (!m_filterEngine.ShouldBackup(filePath)) {
                            return; // Dışlanan glob kuralı
                        }

                        // 2. Ransomware Dedektörü: Entropi hesapla
                        double entropy = CalculateShannonEntropy(filePath);

                        // 3. Hız Eşiği Kontrolü (2 saniyede 10+ yüksek entropili dosya)
                        uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count();

                        bool triggerPanic = false;
                        {
                            std::lock_guard<std::mutex> lock(m_logsMutex);
                            // 2 saniyeden eski kayıtları temizle
                            m_writeLogs.erase(std::remove_if(m_writeLogs.begin(), m_writeLogs.end(),
                                [nowMs](const FileWriteLog& log) { return nowMs - log.timestamp > 2000; }), m_writeLogs.end());

                            m_writeLogs.push_back({nowMs, entropy});

                            int highEntropyCount = 0;
                            for (const auto& log : m_writeLogs) {
                                if (log.entropy > 7.6) {
                                    highEntropyCount++;
                                }
                            }

                            if (highEntropyCount >= 10) {
                                triggerPanic = true;
                            }
                        }

                        if (triggerPanic) {
                            m_panicState = true;
                            m_panicFilePath = filePath;
                            std::wcerr << L"\n[RANSOMWARE PANIK ALARMI] Saldiri algilandi! Gozcu durduruluyor. Dosya: " << filePath << std::endl;
                            m_watcher->Stop(); // Korumayı aktif edip gözcüyü kapat
                            return;
                        }

                        // 4. Yedekleme & Bulut/Ayna Senkronizasyonu
                        std::lock_guard<std::mutex> innerLock(m_storageMutex);
                        if (m_storage.BackupFile(filePath)) {
                            m_cloudSync.TriggerSync(m_storage.GetVaultPath());
                        }
                    }
                });
                
                bool ok = m_watcher->Start();
                std::wcout << L"[IPC] Gozcu baslatildi. Durum: " << (ok ? L"BASARILI" : L"HATA") << std::endl;
                SendResponseStatus(hPipe, ok ? 1 : 0);
                break;
            }
            
            case CMD_STOP_WATCHER: {
                std::wcout << L"[IPC] Gozcu durduruluyor ve Panik durumu sifirlaniyor..." << std::endl;
                std::lock_guard<std::mutex> lock(m_storageMutex);
                if (m_watcher) {
                    m_watcher->Stop();
                    m_watcher.reset();
                    m_watchedPath.clear();
                }
                m_panicState = false;
                m_panicFilePath.clear();
                {
                    std::lock_guard<std::mutex> logLock(m_logsMutex);
                    m_writeLogs.clear();
                }
                std::wcout << L"[IPC] Gozcu durduruldu, durum sifir." << std::endl;
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

            case CMD_GET_STATUS: {
                uint32_t state = m_panicState ? 2 : (m_watcher ? 1 : 0);
                
                std::vector<uint8_t> respPayload;
                respPayload.resize(4);
                std::memcpy(respPayload.data(), &state, 4);

                // İzlenen Dizin Bilgisi
                uint32_t watchLen = static_cast<uint32_t>(m_watchedPath.length());
                size_t offset = respPayload.size();
                respPayload.resize(offset + 4 + watchLen * sizeof(wchar_t));
                std::memcpy(respPayload.data() + offset, &watchLen, 4);
                if (watchLen > 0) {
                    std::memcpy(respPayload.data() + offset + 4, m_watchedPath.data(), watchLen * sizeof(wchar_t));
                }

                // Ransomware Panik Dizin Bilgisi
                uint32_t panicLen = static_cast<uint32_t>(m_panicFilePath.length());
                offset = respPayload.size();
                respPayload.resize(offset + 4 + panicLen * sizeof(wchar_t));
                std::memcpy(respPayload.data() + offset, &panicLen, 4);
                if (panicLen > 0) {
                    std::memcpy(respPayload.data() + offset + 4, m_panicFilePath.data(), panicLen * sizeof(wchar_t));
                }

                IpcHeader respHeader;
                respHeader.magic = 0x4950434D;
                respHeader.command = CMD_RESPONSE;
                respHeader.payload_size = static_cast<uint32_t>(respPayload.size());

                std::wcout << L"[IPC-DEBUG] CMD_GET_STATUS -> state: " << state 
                          << L", watchLen: " << watchLen 
                          << L", panicLen: " << panicLen 
                          << L", payloadSize: " << respPayload.size() << std::endl;

                WriteBytes(hPipe, &respHeader, sizeof(IpcHeader));
                WriteBytes(hPipe, respPayload.data(), static_cast<DWORD>(respPayload.size()));
                break;
            }

            case CMD_SET_RULES: {
                if (payload.size() < 4) {
                    SendResponseStatus(hPipe, 0);
                    break;
                }
                uint32_t ruleCount = 0;
                std::memcpy(&ruleCount, payload.data(), 4);

                std::vector<std::wstring> rules;
                size_t offset = 4;
                bool parseOk = true;

                for (uint32_t i = 0; i < ruleCount; ++i) {
                    if (offset + 4 > payload.size()) { parseOk = false; break; }
                    uint32_t ruleLen = 0;
                    std::memcpy(&ruleLen, payload.data() + offset, 4);
                    offset += 4;

                    if (offset + ruleLen * sizeof(wchar_t) > payload.size()) { parseOk = false; break; }
                    std::wstring rule(reinterpret_cast<const wchar_t*>(payload.data() + offset), ruleLen);
                    rules.push_back(rule);
                    offset += ruleLen * sizeof(wchar_t);
                }

                if (!parseOk) {
                    SendResponseStatus(hPipe, 0);
                    break;
                }

                m_filterEngine.SetRules(rules);
                SendResponseStatus(hPipe, 1);
                break;
            }

            case CMD_GET_VERSION_CONTENT: {
                if (payload.size() < 8) {
                    SendResponseStatus(hPipe, 0);
                    break;
                }
                uint32_t versionIndex = 0;
                uint32_t pathLen = 0;
                std::memcpy(&versionIndex, payload.data(), 4);
                std::memcpy(&pathLen, payload.data() + 4, 4);

                if (payload.size() < 8 + pathLen * sizeof(wchar_t)) {
                    SendResponseStatus(hPipe, 0);
                    break;
                }

                std::wstring originPath(reinterpret_cast<const wchar_t*>(payload.data() + 8), pathLen);

                std::vector<uint8_t> versionContent;
                bool ok = false;
                {
                    std::lock_guard<std::mutex> lock(m_storageMutex);
                    ok = m_storage.GetVersionContent(originPath, versionIndex, versionContent);
                }

                if (!ok) {
                    SendResponseStatus(hPipe, 0);
                    break;
                }

                IpcHeader respHeader;
                respHeader.magic = 0x4950434D; // 'IPCM' ASCII
                respHeader.command = CMD_RESPONSE;
                respHeader.payload_size = static_cast<uint32_t>(versionContent.size());

                if (WriteBytes(hPipe, &respHeader, sizeof(IpcHeader)) &&
                    WriteBytes(hPipe, versionContent.data(), static_cast<DWORD>(versionContent.size()))) {
                    std::wcout << L"[IPC] Surum icerigi yaniti gonderildi. Boyut: " << versionContent.size() << L" byte." << std::endl;
                }
                break;
            }
            
            default:
                std::wcerr << L"[IPC-HATA] Bilinmeyen Komut ID: " << header.command << std::endl;
                SendResponseStatus(hPipe, 0); // Bilinmeyen Komut
                break;
        }
        std::wcout << L"[IPC-DEBUG] HandleClient: Istek islendi, dongu devam ediyor..." << std::endl;
    }
    std::wcout << L"[IPC-DEBUG] HandleClient: Istek dongusunden cikildi, baglanti sonlandiriliyor." << std::endl;
}

double IpcServer::CalculateShannonEntropy(const std::wstring& filePath) {
    std::ifstream file(WStringToANSI(filePath), std::ios::binary);
    if (!file.is_open()) return 0.0;

    std::vector<uint8_t> buffer(2048);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    std::streamsize bytesRead = file.gcount();
    if (bytesRead <= 0) return 0.0;

    std::array<double, 256> frequencies = {0.0};
    for (std::streamsize i = 0; i < bytesRead; ++i) {
        frequencies[buffer[i]]++;
    }

    double entropy = 0.0;
    for (double freq : frequencies) {
        if (freq > 0.0) {
            double p = freq / bytesRead;
            entropy -= p * (std::log2(p));
        }
    }
    return entropy;
}
