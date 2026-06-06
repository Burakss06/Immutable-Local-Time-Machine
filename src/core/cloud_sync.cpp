#include "cloud_sync.h"
#include "win32_utils.h"
#include <wininet.h>
#include <iostream>
#include <fstream>
#include <sstream>

#pragma comment(lib, "wininet.lib")

// URL Ayrıştırıcı Yardımcı Fonksiyon
static bool ParseUrl(const std::wstring& url, bool& isHttps, std::wstring& host, int& port, std::wstring& path) {
    isHttps = false;
    std::wstring prefix;
    if (url.compare(0, 8, L"https://") == 0) {
        isHttps = true;
        prefix = L"https://";
    } else if (url.compare(0, 7, L"http://") == 0) {
        prefix = L"http://";
    } else {
        return false;
    }

    std::wstring urlWithoutPrefix = url.substr(prefix.length());
    size_t slashPos = urlWithoutPrefix.find(L'/');
    std::wstring hostAndPort = (slashPos == std::wstring::npos) ? urlWithoutPrefix : urlWithoutPrefix.substr(0, slashPos);
    path = (slashPos == std::wstring::npos) ? L"/" : urlWithoutPrefix.substr(slashPos);

    size_t colonPos = hostAndPort.find(L':');
    if (colonPos == std::wstring::npos) {
        host = hostAndPort;
        port = isHttps ? 443 : 80;
    } else {
        host = hostAndPort.substr(0, colonPos);
        try {
            port = std::stoi(hostAndPort.substr(colonPos + 1));
        } catch (...) {
            port = isHttps ? 443 : 80;
        }
    }
    return true;
}

CloudSyncBridge::CloudSyncBridge() : m_syncMode(0), m_syncInProgress(false) {
    LoadConfig();
}

CloudSyncBridge::~CloudSyncBridge() {}

void CloudSyncBridge::LoadConfig() {
    std::lock_guard<std::mutex> lock(m_configMutex);
    std::ifstream file("cloud_config.txt");
    if (!file.is_open()) {
        m_syncMode = 0;
        m_syncTarget.clear();
        return;
    }
    
    std::string modeLine;
    std::string targetLine;
    if (std::getline(file, modeLine) && std::getline(file, targetLine)) {
        try {
            m_syncMode = std::stoi(modeLine);
        } catch (...) {
            m_syncMode = 0;
        }
        m_syncTarget = ANSIToWString(targetLine);
    }
}

void CloudSyncBridge::SaveConfig(int mode, const std::wstring& target) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_syncMode = mode;
    m_syncTarget = target;

    std::ofstream file("cloud_config.txt");
    if (file.is_open()) {
        file << mode << "\n" << WStringToANSI(target) << "\n";
    }
}

void CloudSyncBridge::TriggerSync(const std::wstring& vaultPath) {
    if (m_syncMode == 0) return; // Devredışı
    if (m_syncInProgress) return; // Zaten çalışıyor

    m_syncInProgress = true;
    std::thread([this, vaultPath]() {
        WorkerThread(vaultPath);
    }).detach();
}

void CloudSyncBridge::WorkerThread(std::wstring vaultPath) {
    int mode = 0;
    std::wstring target;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        mode = m_syncMode;
        target = m_syncTarget;
    }

    bool success = false;
    std::wcout << L"\n[BULUT] Senkronizasyon tetiklendi (Mod: " << mode << L")..." << std::endl;

    if (mode == 1) {
        success = SyncToLocalMirror(vaultPath, target);
    } else if (mode == 2) {
        success = SyncToHttpCloud(vaultPath, target);
    }

    if (success) {
        std::wcout << L"[BULUT] Senkronizasyon BASARILI!" << std::endl;
    } else {
        std::wcerr << L"[BULUT-HATA] Senkronizasyon basarisiz oldu!" << std::endl;
    }
    m_syncInProgress = false;
}

bool CloudSyncBridge::SyncToLocalMirror(const std::wstring& srcPath, const std::wstring& destPath) {
    // Hedef klasör yolunun sonuna dosya adını ekleyelim
    std::wstring targetFile = destPath;
    if (!targetFile.empty() && targetFile.back() != L'\\' && targetFile.back() != L'/') {
        targetFile += L'\\';
    }
    targetFile += L"vault.bin";

    // Dosyayı basit binary kopyalama yöntemiyle kopyalayalım
    std::ifstream src(WStringToANSI(srcPath), std::ios::binary);
    std::ofstream dst(WStringToANSI(targetFile), std::ios::binary);
    if (!src.is_open() || !dst.is_open()) return false;

    dst << src.rdbuf();
    return dst.good();
}

bool CloudSyncBridge::SyncToHttpCloud(const std::wstring& srcPath, const std::wstring& urlStr) {
    bool isHttps = false;
    std::wstring host;
    int port = 80;
    std::wstring path;
    if (!ParseUrl(urlStr, isHttps, host, port, path)) {
        std::wcerr << L"[BULUT-HATA] Gecersiz URL formatı: " << urlStr << std::endl;
        return false;
    }

    // Dosyayı okuyalım
    std::ifstream file(WStringToANSI(srcPath), std::ios::binary);
    if (!file.is_open()) return false;
    
    std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    HINTERNET hSession = InternetOpenW(L"ILTM_CloudSync/1.0", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hSession) return false;

    HINTERNET hConnect = InternetConnectW(hSession, host.c_str(), port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hSession);
        return false;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (isHttps) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hRequest = HttpOpenRequestW(hConnect, L"PUT", path.c_str(), nullptr, nullptr, nullptr, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hSession);
        return false;
    }

    // İstek gönderilir (PUT payload olarak)
    BOOL sent = HttpSendRequestW(hRequest, nullptr, 0, fileData.data(), static_cast<DWORD>(fileData.size()));
    
    // HTTP Durum Kodunu oku
    DWORD statusCode = 0;
    DWORD statusCodeLen = sizeof(statusCode);
    HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeLen, nullptr);

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hSession);

    // S3 PUT genellikle 200 OK döner
    return sent && (statusCode >= 200 && statusCode < 300);
}
